#include "DirectoryValueTree.h"
#include "DebugLog.h"
#include "RuntimeRootProperties.h"
#include "ValueTreeHelpers.h"
#include <algorithm>

#define LOG_DIRECTORY_VALUE_TREE 0
#if LOG_DIRECTORY_VALUE_TREE
#define LogDirectoryValueTree(cond, text) if (cond) { DebugLog ("DirectoryValueTree", text); }
#else
#define LogDirectoryValueTree(cond, text) ;
#endif

#define SHOW_CHECK_STATE_LOG false
#define SHOW_TASK_MANAGEMENT_LOG false

DirectoryValueTree::DirectoryValueTree () : Thread ("DirectoryValueTree")
{
    startThread ();
    scanThread.onThreadLoop = [this] ()
    {
        if (! scanThread.waitForNotification (-1) || scanThread.shouldExit ())
            return false;

        LogDirectoryValueTree (true, "scanThread.onThreadLoop - calling scanDirectory ()");
        scanDirectory ();
        setTaskCompleteIfNoScanPending ();
        wakeUpTaskManagmentThread ();
        sendStatusUpdate (DirectoryDataProperties::ScanStatus::done);
        doProgressUpdate ("");

        return true;
    };
    scanThread.start ();
    checkThread.onThreadLoop = [this] ()
    {
        if (! checkThread.waitForNotification (-1) || checkThread.shouldExit ())
            return false;

        LogDirectoryValueTree (SHOW_CHECK_STATE_LOG, "checkThread.onThreadLoop - TaskManagementState::checking");
        if (hasFolderChanged ())
        {
            scanRequestPending = true;
            setRequestedTaskManagementState (TaskManagementState::startScan);
            wakeUpTaskManagmentThread ();
        }
        else
        {
            setTaskCompleteIfNoScanPending ();
            wakeUpTaskManagmentThread ();
        }

        return true;
    };
    checkThread.start ();
}

DirectoryValueTree::~DirectoryValueTree ()
{
    // the scan and check threads use members of this object, including taskManagementCS and rootFolderNameCS, which are
    // declared after them and are therefore destroyed before them. left to the LambdaThread destructors, those threads
    // would still be running while those members were being torn down, and a task reporting its completion would take a
    // lock that no longer exists. so everything that can call back into this object is shut down here, in order, while
    // all of it is still alive
    stopTimer ();
    // let any scan/check that is part way through give up promptly, instead of running to completion
    cancelScan = true;
    cancelCheck = true;
    // the task management thread first, so that it cannot wake the workers back up after they have been stopped
    stopThread (500);
    scanThread.stop ();
    checkThread.stop ();
}

void DirectoryValueTree::wakeUpTaskManagmentThread ()
{
    LogDirectoryValueTree (SHOW_TASK_MANAGEMENT_LOG, "wakeUpTaskManagmentThread");
    notify ();
}

void DirectoryValueTree::init (juce::ValueTree runtimeRootPropertiesVT)
{
    directoryDataProperties.wrap (runtimeRootPropertiesVT, DirectoryDataProperties::WrapperType::owner, DirectoryDataProperties::EnableCallbacks::yes);
    //ddpMonitor.assign (directoryDataProperties.getValueTreeRef ());

    // capture a handle to the live root folder tree while on the message thread. the scan thread only uses it
    // to pass the live tree to publish operations that run on the message thread
    rootFolderVTForTask = directoryDataProperties.getRootFolderVT ();

    // the two types that exist without being registered. folders are identified structurally, and
    // unknown is the fallback when no predicate matches, so neither carries a predicate
    fileTypes.clear ();
    fileTypes.push_back ({ unknownTypeId, "unknown", nullptr, nullptr, nullptr });
    fileTypes.push_back ({ folderTypeId,  "folder",  nullptr, nullptr, nullptr });
    sortOrder.clear ();
    initialised = true;
    publishFileTypes ();

    directoryDataProperties.onScanDepthChange = [this] (int scanDepth) { setScanDepth (scanDepth); };
    directoryDataProperties.onStartScanChange = [this] ()
    {
        FolderProperties fp (directoryDataProperties.getRootFolderVT (), FolderProperties::WrapperType::client, FolderProperties::EnableCallbacks::no);
        LogDirectoryValueTree (true, "init - directoryDataProperties.onStartScanChange - " + fp.getName ());
        startScan ();
    };

    startTimer (250);
}

juce::ValueTree DirectoryValueTree::getDirectoryDataPropertiesVT ()
{
    return directoryDataProperties.getValueTree ();
}

void DirectoryValueTree::setScanDepth (int theScanDepth)
{
    scanDepth = theScanDepth;
}

void DirectoryValueTree::doIfProgressTimeElapsed (std::function<void ()> functionToDo)
{
    jassert (functionToDo != nullptr);
    if (juce::Time::currentTimeMillis () - lastScanInProgressUpdate > 250)
    {
        lastScanInProgressUpdate = juce::Time::currentTimeMillis ();
        functionToDo ();
    }
}

void DirectoryValueTree::sendStatusUpdate (DirectoryDataProperties::ScanStatus scanStatus)
{
    juce::MessageManager::callAsync ([this, scanStatus] ()
    {
        //juce::Logger::outputDebugString ("setting scanStatus: " + juce::String (static_cast<int> (scanStatus)));
        directoryDataProperties.setStatus (scanStatus, false);
    });
}

juce::ValueTree DirectoryValueTree::makeFileEntry (juce::File file, juce::int64 createTime, juce::int64 modificationTime, int fileType)
{
    auto fileVT { FileProperties::create (file.getFullPathName (), createTime, modificationTime, fileType) };
    // decorators can be expensive (reading file headers and such), and a check scan only compares names and
    // timestamps, so they only run for a full scan
    if (scanType == ScanType::fullScan)
    {
        if (const auto* fileTypeInfo { getFileTypeInfo (fileType) }; fileTypeInfo != nullptr && fileTypeInfo->decorator != nullptr)
            fileTypeInfo->decorator (fileVT, file);
    }
    return fileVT;
}

bool DirectoryValueTree::shouldCancelOperation (LambdaThread& whichTaskThread, std::atomic<bool>& whichTaskCancelToCheck)
{
    return whichTaskThread.shouldExit () || whichTaskCancelToCheck;
}

void DirectoryValueTree::startScan ()
{
    LogDirectoryValueTree (true, "startScan - waking up scan thread");
    // called on the message thread, where reading the live tree is safe. capture the root folder name here,
    // since the scan/check threads cannot safely read it from the live tree themselves
    jassert (juce::MessageManager::existsAndIsCurrentThread ());
    FolderProperties rootFolderProperties (directoryDataProperties.getRootFolderVT (), FolderProperties::WrapperType::client, FolderProperties::EnableCallbacks::no);
    jassert (! rootFolderProperties.getName ().isEmpty ());
    {
        juce::ScopedLock sl (rootFolderNameCS);
        rootFolderTaskName = rootFolderProperties.getName ();
    }
    scanEverStarted = true;
    scanRequestPending = true;
    setRequestedTaskManagementState (TaskManagementState::startScan);
    wakeUpTaskManagmentThread ();
}

void DirectoryValueTree::setTaskCompleteIfNoScanPending ()
{
    // a scan that was requested while this task was running has not been started yet, so its request must be
    // left in place. without this, a task finishing right after a request would report idle over it, and the
    // requested scan would never happen
    if (scanRequestPending)
        return;
    setRequestedTaskManagementState (TaskManagementState::idle);
}

juce::String DirectoryValueTree::getRootFolderTaskName ()
{
    juce::ScopedLock sl (rootFolderNameCS);
    return rootFolderTaskName;
}

void DirectoryValueTree::timerCallback ()
{
    LogDirectoryValueTree (SHOW_CHECK_STATE_LOG, "timerCallback - doChangeCheck");
    if (setRequestedTaskManagementState (TaskManagementState::startCheck))
        wakeUpTaskManagmentThread ();
}

DirectoryValueTree::TaskManagementState DirectoryValueTree::getCurrentTaskManagementState ()
{
    juce::ScopedLock sl (taskManagementCS);
    return currentTaskManagementState;
}

juce::String DirectoryValueTree::getTaskManagementStateString (TaskManagementState theTaskMangementState)
{
    switch (theTaskMangementState)
    {
        case TaskManagementState::idle: return "idle"; break;
        case TaskManagementState::startScan: return "startScan"; break;
        case TaskManagementState::scanning: return "scanning"; break;
        case TaskManagementState::startCheck: return "startCheck"; break;
        case TaskManagementState::checking: return "checking"; break;
        default: jassertfalse; return ""; break;
    }
}

void DirectoryValueTree::setCurrentTaskManagementState (TaskManagementState newTaskManagementState)
{
    juce::ScopedLock sl (taskManagementCS);
    LogDirectoryValueTree (SHOW_TASK_MANAGEMENT_LOG, "setCurrentTaskManagementState: " + getTaskManagementStateString (newTaskManagementState));
    currentTaskManagementState = newTaskManagementState;
}

DirectoryValueTree::TaskManagementState DirectoryValueTree::getRequestedTaskManagementState ()
{
    juce::ScopedLock sl (taskManagementCS);
    return requestedTaskManagementState;
}

bool DirectoryValueTree::setRequestedTaskManagementState (DirectoryValueTree::TaskManagementState newTaskManagementState)
{
    juce::ScopedLock sl (taskManagementCS);
    // since the check process is running every X milliseconds, we can skip it if we aren't idle
    if (newTaskManagementState == TaskManagementState::startCheck && currentTaskManagementState != TaskManagementState::idle)
    {
        LogDirectoryValueTree (SHOW_TASK_MANAGEMENT_LOG, "setRequestedTaskManagementState - skipping TaskManagementState::startCheck because currentTaskMangementState == " +
                                     getTaskManagementStateString (currentTaskManagementState));
        return false;
    }
    LogDirectoryValueTree (SHOW_TASK_MANAGEMENT_LOG, "setRequestedTaskManagementState - requesting: " + getTaskManagementStateString (newTaskManagementState));
    requestedTaskManagementState = newTaskManagementState;
    return true;
}

void DirectoryValueTree::run ()
{
    LogDirectoryValueTree (SHOW_TASK_MANAGEMENT_LOG, "run - enter");
    while (! threadShouldExit ())
    {
        // while we are waiting for a task to notice that it has been cancelled, the wait is bounded so that we come
        // back and re-check. the task can finish of its own accord before it ever sees the flag, in which case there
        // is no wake up coming, and a pending scan request would sit here forever
        wait ((cancelScan || cancelCheck) ? 10 : -1);
        if (threadShouldExit ())
            break;
        const auto requestedTMS { getRequestedTaskManagementState () };
        setCurrentTaskManagementState (requestedTMS);
        LogDirectoryValueTree (SHOW_TASK_MANAGEMENT_LOG, "run - currentTaskMangementState == " + getTaskManagementStateString (requestedTMS));
        switch (requestedTMS)
        {
            case TaskManagementState::idle:
            {
                // spurious wake up?
                //jassertfalse;
            }
            break;
            case TaskManagementState::startScan:
            {
                if (! checkThread.isWaiting ())
                {
                    LogDirectoryValueTree (SHOW_TASK_MANAGEMENT_LOG, "run - check thread is still running. waiting for completion");
                    // the flag stays set until the check has actually stopped. clearing it before then, as the
                    // async update used to, means the check never sees it and can never be cancelled
                    cancelCheck = true;
                }
                else if (! scanThread.isWaiting ())
                {
                    LogDirectoryValueTree (SHOW_TASK_MANAGEMENT_LOG, "run - scan thread is still running. waiting for completion");
                    cancelScan = true;
                }
                else
                {
                    LogDirectoryValueTree (SHOW_TASK_MANAGEMENT_LOG, "run - starting scan thread");
                    // nothing is running any more, so the cancel requests have been served
                    cancelCheck = false;
                    cancelScan = false;
                    scanRequestPending = false;
                    // the request has been acted on. left as startScan, the next wake up of this thread would come
                    // back through here, find the scan thread busy, and cancel the scan that was just started
                    setRequestedTaskManagementState (TaskManagementState::scanning);
                    setCurrentTaskManagementState (TaskManagementState::scanning);
                    sendStatusUpdate (DirectoryDataProperties::ScanStatus::scanning);
                    scanThread.wake ();
                }
            }
            break;
            case TaskManagementState::scanning:
            {
                // the scan thread is running. it wakes this thread again when it is done
            }
            break;
            case TaskManagementState::startCheck:
            {
                // as this is a time repeated task, we can skip it if anything else is already going on
                if (scanThread.isWaiting () && checkThread.isWaiting ())
                {
                    LogDirectoryValueTree (SHOW_TASK_MANAGEMENT_LOG, "run - starting check thread");
                    setRequestedTaskManagementState (TaskManagementState::checking);
                    setCurrentTaskManagementState (TaskManagementState::checking);
                    checkThread.wake ();
                }
            }
            break;
            case TaskManagementState::checking:
            {
                // TODO - I don't think we should ever end up here
                //jassertfalse;
            }
            break;
        }
    }
    LogDirectoryValueTree (true, "run - exit");
}

juce::String DirectoryValueTree::getPathFromCurrentRoot (juce::String fullPath)
{
    const auto partialPath { fullPath.fromLastOccurrenceOf (getRootFolderTaskName (), false, true) };
    return partialPath;
}

void DirectoryValueTree::scanDirectory ()
{
    LogDirectoryValueTree (true, "scanDirectory ()");
    lastScanInProgressUpdate = juce::Time::currentTimeMillis ();
    const auto rootFolderName { getRootFolderTaskName () };
    // do one initial progress update to fill in the first one
    doProgressUpdate ("Reading File System: " + getPathFromCurrentRoot (juce::File (rootFolderName).getFileName ()));
    timer.start (100000);
    // scan into a detached tree, since the live tree (which has listeners) may only be modified on the message thread
    FolderProperties scannedFolderProperties ({}, FolderProperties::WrapperType::owner, FolderProperties::EnableCallbacks::no);
    scannedFolderProperties.setName (rootFolderName, false);
    scanType = ScanType::fullScan;
    getContentsOfFolder (scannedFolderProperties.getValueTree (), 0, [this] () { return shouldCancelOperation (scanThread, cancelScan); });
    if (! shouldCancelOperation (scanThread, cancelScan))
    {
        // keep a detached copy for the check thread to compare against
        lastScanResultVT = scannedFolderProperties.getValueTree ().createCopy ();
        // publish the scanned data into the live tree on the message thread. this thread must not touch the scanned tree after this
        ValueTreeHelpers::replaceChildrenOnMessageThread (rootFolderVTForTask, scannedFolderProperties.getValueTree ());
    }
    else
    {
        LogDirectoryValueTree (true, "scanDirectory - operation cancelled, removing all data");
        lastScanResultVT = {};
        ValueTreeHelpers::callOnMessageThread ([liveRootFolderVT = rootFolderVTForTask] () mutable { liveRootFolderVT.removeAllChildren (nullptr); });
    }
    //juce::Logger::outputDebugString ("DirectoryValueTree::scanDirectory ()- elapsed time: " + juce::String (timer.getElapsedTime ()));
}

void DirectoryValueTree::doProgressUpdate (juce::String progressString)
{
    juce::MessageManager::callAsync ([this, progressString] ()
    {
        directoryDataProperties.setProgress (progressString, false);
    });
}

bool DirectoryValueTree::hasFolderChanged ()
{
    // runs on the check thread, so it compares the file system against a detached copy of the last scan result,
    // since the live tree may only be safely accessed from the message thread
    if (! lastScanResultVT.isValid ())
        return false;
    FolderProperties rootFolderProperties (lastScanResultVT, FolderProperties::WrapperType::client, FolderProperties::EnableCallbacks::no);
    // the folder being viewed can change while we are idle, in which case the last scan result describes some other
    // folder, and the contents need to be rescanned
    const auto currentRootFolderName { getRootFolderTaskName () };
    if (rootFolderProperties.getName () != currentRootFolderName)
    {
        LogDirectoryValueTree (SHOW_CHECK_STATE_LOG, "hasFolderChanged - root folder changed - do rescan");
        return true;
    }
    FolderProperties newCopyOfFolderProperties ({}, FolderProperties::WrapperType::owner, FolderProperties::EnableCallbacks::no);
    newCopyOfFolderProperties.setName (currentRootFolderName, false);
    scanType = ScanType::checkForUpdate;
    getContentsOfFolder (newCopyOfFolderProperties.getValueTree (), 0, [this] () { return shouldCancelOperation (checkThread, cancelCheck); });
    if (rootFolderProperties.getValueTree ().getNumChildren () != newCopyOfFolderProperties.getValueTree ().getNumChildren ())
    {
        LogDirectoryValueTree (SHOW_CHECK_STATE_LOG, "hasFolderChanged - number of children differ - do rescan");
        return true;
    }
    for (auto childIndex { 0 }; childIndex < rootFolderProperties.getValueTree ().getNumChildren (); ++childIndex)
    {
        auto rootChildVT { rootFolderProperties.getValueTree ().getChild (childIndex) };
        auto newChildVT { newCopyOfFolderProperties.getValueTree ().getChild (childIndex) };
        if (FolderProperties::isFolderVT (rootChildVT))
        {
            FolderProperties curRootFolderChildFolder (rootChildVT, FolderProperties::WrapperType::owner, FolderProperties::EnableCallbacks::no);
            if (! FolderProperties::isFolderVT (newChildVT))
            {
                LogDirectoryValueTree (SHOW_CHECK_STATE_LOG, "hasFolderChanged - item #" + juce::String (childIndex) + " differ in type - do rescan");
                return true;
            }
            else
            {
                FolderProperties curNewFolderChildFolder (newChildVT, FolderProperties::WrapperType::owner, FolderProperties::EnableCallbacks::no);
                if (curRootFolderChildFolder.getName () != curNewFolderChildFolder.getName ())
                {
                    LogDirectoryValueTree (SHOW_CHECK_STATE_LOG, "hasFolderChanged - item #" + juce::String (childIndex) + " names differ - do rescan");
                    return true;
                }
                if (curRootFolderChildFolder.getCreateTime () != curNewFolderChildFolder.getCreateTime ())
                {
                    LogDirectoryValueTree (SHOW_CHECK_STATE_LOG, "hasFolderChanged - item #" + juce::String (childIndex) + " creation times differ - do rescan");
                    return true;
                }
                if (curRootFolderChildFolder.getModificationTime () != curNewFolderChildFolder.getModificationTime ())
                {
                    LogDirectoryValueTree (SHOW_CHECK_STATE_LOG, "hasFolderChanged - item #" + juce::String (childIndex) + " modification times differ - do rescan");
                    return true;
                }
            }
        }
        else
        {
            FileProperties curRootFolderChildFile (rootChildVT, FileProperties::WrapperType::owner, FileProperties::EnableCallbacks::no);
            if (! FileProperties::isFileVT (newChildVT))
            {
                LogDirectoryValueTree (SHOW_CHECK_STATE_LOG, "hasFolderChanged - item #" + juce::String (childIndex) + " differ in type - do rescan");
                return true;
            }
            else
            {
                FileProperties curNewFolderChildFile (newChildVT, FileProperties::WrapperType::owner, FileProperties::EnableCallbacks::no);
                if (curRootFolderChildFile.getName () != curNewFolderChildFile.getName ())
                {
                    LogDirectoryValueTree (SHOW_CHECK_STATE_LOG, "hasFolderChanged - item #" + juce::String (childIndex) + " names differ - do rescan");
                    return true;
                }
                if (curRootFolderChildFile.getCreateTime () != curNewFolderChildFile.getCreateTime ())
                {
                    LogDirectoryValueTree (SHOW_CHECK_STATE_LOG, "hasFolderChanged - item #" + juce::String (childIndex) + " creation times differ - do rescan");
                    return true;
                }
                if (curRootFolderChildFile.getModificationTime () != curNewFolderChildFile.getModificationTime ())
                {
                    LogDirectoryValueTree (SHOW_CHECK_STATE_LOG, "hasFolderChanged - item #" + juce::String (childIndex) + " modification times differ - do rescan");
                    return true;
                }

            }
        }
    }
    return false;
}

void DirectoryValueTree::getContentsOfFolder (juce::ValueTree folderVT, int curDepth, std::function<bool ()> shouldCancelFunc)
{
    FolderProperties folderProperties (folderVT, FolderProperties::WrapperType::client, FolderProperties::EnableCallbacks::no);
    if (scanDepth == -1 || curDepth <= scanDepth)
    {
        for (const auto& entry : juce::RangedDirectoryIterator (folderProperties.getName (), false, "*", juce::File::findFilesAndDirectories))
        {
            if (shouldCancelFunc ())
                break;

            const auto creationTime { entry.getFile ().getCreationTime ().getMilliseconds () };
            const auto modificationTime { entry.getFile ().getLastModificationTime ().getMilliseconds () };
            if (scanType == ScanType::fullScan)
                doIfProgressTimeElapsed ([this, fileName = entry.getFile ().getFileName ()] () { doProgressUpdate ("Reading File System: " + getPathFromCurrentRoot (fileName)); });
            if (const auto& curFile { entry.getFile () }; curFile.isDirectory ())
                folderVT.addChild (FolderProperties::create (curFile.getFullPathName (), creationTime, modificationTime), -1, nullptr);
            else
                folderVT.addChild (makeFileEntry (curFile, creationTime, modificationTime, identifyFileType (curFile)), -1, nullptr);
        }
        sortContentsOfFolder (folderVT, shouldCancelFunc);
        if (scanType == ScanType::fullScan && curDepth == 0)
        {
            // publish a snapshot of the root level results right away (they were previously available immediately,
            // since the scan used to write directly into the live tree). copied, because this thread continues
            // to fill in the subfolders of folderVT
            ValueTreeHelpers::replaceChildrenOnMessageThread (rootFolderVTForTask, folderVT.createCopy (), [this] ()
            {
                directoryDataProperties.triggerRootScanComplete (false);
            });
        }

        // scan the subfolders
        ValueTreeHelpers::forEachChildOfType (folderVT, FolderProperties::FolderTypeId, [this, curDepth, shouldCancelFunc] (juce::ValueTree childFolderVT)
        {
            getContentsOfFolder (childFolderVT, curDepth + 1, shouldCancelFunc);
            return true;
        });
    }
}

void DirectoryValueTree::sortContentsOfFolder (juce::ValueTree rootFolderVT, std::function<bool ()> shouldCancelFunc)
{
    jassert (FolderProperties::isFolderVT (rootFolderVT));

    // entries are grouped into one section per file type, laid out in the order given by setSortOrder,
    // and alphabetized within each section by that type's comparator
    struct SectionInfo
    {
        int startIndex { 0 };
        int length { 0 };
    };
    const auto numSections { static_cast<int> (sortOrder.size ()) };
    jassert (numSections > 0);
    std::vector<SectionInfo> sections (static_cast<size_t> (numSections));
    const auto numFolderEntries { rootFolderVT.getNumChildren () };

    auto insertSorted = [this, &sections, &rootFolderVT, numSections] (int itemIndex, int typeId)
    {
        auto getEntryName = [] (juce::ValueTree dirEntryVT)
        {
            return dirEntryVT.getProperty ("name").toString ();
        };

        const auto sectionIndex { getSectionForType (typeId) };
        jassert (sectionIndex >= 0 && sectionIndex < numSections);
        auto& section { sections [static_cast<size_t> (sectionIndex)] };
        jassert (itemIndex >= section.startIndex + section.length);
        auto startingSectionLength { section.length };
        auto insertItem = [&rootFolderVT, &section, &sections, numSections] (int itemIndex, int insertIndex)
        {
            auto tempVT { rootFolderVT.getChild (itemIndex) };
            rootFolderVT.removeChild (itemIndex, nullptr);
            rootFolderVT.addChild (tempVT, insertIndex, nullptr);
            ++section.length;
            for (auto curSectionIndex { 1 }; curSectionIndex < numSections; ++curSectionIndex)
                sections [static_cast<size_t> (curSectionIndex)].startIndex = sections [static_cast<size_t> (curSectionIndex - 1)].startIndex +
                                                                             sections [static_cast<size_t> (curSectionIndex - 1)].length;
        };

        const auto entryName { getEntryName (rootFolderVT.getChild (itemIndex)) };
        for (auto sectionEntryIndex { section.startIndex }; sectionEntryIndex < section.startIndex + section.length; ++sectionEntryIndex)
        {
            if (compareEntryNames (typeId, entryName, getEntryName (rootFolderVT.getChild (sectionEntryIndex))))
            {
                insertItem (itemIndex, sectionEntryIndex);
                break;
            }
        }
        if (section.length == startingSectionLength)
            insertItem (itemIndex, section.startIndex + section.length);
    };
    for (auto folderIndex { 0 }; folderIndex < numFolderEntries && ! shouldCancelFunc (); ++folderIndex)
    {
        auto directoryEntryVT { rootFolderVT.getChild (folderIndex) };
        if (scanType == ScanType::fullScan)
            doIfProgressTimeElapsed ([this, fileName = directoryEntryVT.getProperty ("name").toString ()] () { doProgressUpdate ("Sorting File System: " + getPathFromCurrentRoot (fileName)); });
        if (FolderProperties::isFolderVT (directoryEntryVT))
        {
            insertSorted (folderIndex, folderTypeId);
        }
        else if (FileProperties::isFileVT (directoryEntryVT))
        {
            const auto typeId { static_cast<int> (directoryEntryVT.getProperty (FileProperties::TypePropertyId)) };
            insertSorted (folderIndex, getFileTypeInfo (typeId) != nullptr ? typeId : unknownTypeId);
        }
        else
        {
            jassertfalse;
        }
    }
}

int DirectoryValueTree::registerFileType (juce::String typeName, FileTypePredicate predicate,
                                          EntryDecorator decorator, EntryComparator comparator)
{
    // init () seeds the built-in types, and re-wraps directoryDataProperties, which would discard anything
    // published before it ran
    jassert (initialised);
    jassert (predicate != nullptr);
    jassert (! typeName.isEmpty ());
    // registering once scanning has begun would resize the section list underneath a running sort
    jassert (! scanEverStarted);
    // a duplicate name would make DirectoryDataProperties::getFileTypeId ambiguous
    jassert (std::none_of (fileTypes.begin (), fileTypes.end (), [typeName] (const FileTypeInfo& fileTypeInfo) { return fileTypeInfo.name == typeName; }));

    const auto typeId { static_cast<int> (fileTypes.size ()) };
    fileTypes.push_back ({ typeId, typeName, predicate, decorator, comparator });
    // a sort order established earlier does not name this type, so it is discarded in favour of the default
    sortOrder.clear ();
    publishFileTypes ();
    return typeId;
}

void DirectoryValueTree::setComparatorForType (int typeId, EntryComparator comparator)
{
    jassert (initialised);
    jassert (typeId >= 0 && typeId < static_cast<int> (fileTypes.size ()));
    if (typeId < 0 || typeId >= static_cast<int> (fileTypes.size ()))
        return;
    fileTypes [static_cast<size_t> (typeId)].comparator = comparator;
}

void DirectoryValueTree::setSortOrder (std::vector<int> newSortOrder)
{
    jassert (initialised);
    jassert (! scanEverStarted);
    // every known type must appear exactly once, or entries of a missing type would have nowhere to go
    jassert (newSortOrder.size () == fileTypes.size ());
    for (const auto& fileTypeInfo : fileTypes)
        jassert (std::count (newSortOrder.begin (), newSortOrder.end (), fileTypeInfo.id) == 1);

    applySortOrder (std::move (newSortOrder));
    publishFileTypes ();
}

void DirectoryValueTree::applySortOrder (std::vector<int> newSortOrder)
{
    sortOrder = std::move (newSortOrder);
    sectionForTypeId.assign (fileTypes.size (), -1);
    for (auto sectionIndex { 0 }; sectionIndex < static_cast<int> (sortOrder.size ()); ++sectionIndex)
    {
        const auto typeId { sortOrder [static_cast<size_t> (sectionIndex)] };
        if (typeId >= 0 && typeId < static_cast<int> (sectionForTypeId.size ()))
            sectionForTypeId [static_cast<size_t> (typeId)] = sectionIndex;
    }
}

void DirectoryValueTree::useDefaultSortOrderIfUnset ()
{
    if (! sortOrder.empty ())
        return;
    // folders first, then the registered types in registration order, then unknown. unknown goes last
    // because it is the fallback, and entries that nothing claimed are the least interesting to show first
    std::vector<int> defaultSortOrder;
    defaultSortOrder.push_back (folderTypeId);
    for (auto typeId { folderTypeId + 1 }; typeId < static_cast<int> (fileTypes.size ()); ++typeId)
        defaultSortOrder.push_back (typeId);
    defaultSortOrder.push_back (unknownTypeId);
    applySortOrder (std::move (defaultSortOrder));
}

int DirectoryValueTree::identifyFileType (juce::File file)
{
    // the first two entries are the built-ins, which have no predicate. registration order is the order the
    // predicates are tried in, so clients control which test runs first
    for (auto typeId { folderTypeId + 1 }; typeId < static_cast<int> (fileTypes.size ()); ++typeId)
    {
        const auto& fileTypeInfo { fileTypes [static_cast<size_t> (typeId)] };
        if (fileTypeInfo.predicate != nullptr && fileTypeInfo.predicate (file))
            return fileTypeInfo.id;
    }
    return unknownTypeId;
}

const DirectoryValueTree::FileTypeInfo* DirectoryValueTree::getFileTypeInfo (int typeId) const
{
    if (typeId < 0 || typeId >= static_cast<int> (fileTypes.size ()))
        return nullptr;
    return &fileTypes [static_cast<size_t> (typeId)];
}

int DirectoryValueTree::getSectionForType (int typeId) const
{
    if (typeId < 0 || typeId >= static_cast<int> (sectionForTypeId.size ()))
        return -1;
    return sectionForTypeId [static_cast<size_t> (typeId)];
}

bool DirectoryValueTree::compareEntryNames (int typeId, juce::String firstName, juce::String secondName) const
{
    if (const auto* fileTypeInfo { getFileTypeInfo (typeId) }; fileTypeInfo != nullptr && fileTypeInfo->comparator != nullptr)
        return fileTypeInfo->comparator (firstName, secondName);
    return firstName.toLowerCase () < secondName.toLowerCase ();
}

void DirectoryValueTree::publishFileTypes ()
{
    // the registry is read by clients through DirectoryDataProperties, so it lives in the tree rather than
    // being handed around by the registering code
    useDefaultSortOrderIfUnset ();
    auto fileTypesVT { directoryDataProperties.getFileTypesVT () };
    fileTypesVT.removeAllChildren (nullptr);
    for (const auto& fileTypeInfo : fileTypes)
        fileTypesVT.addChild (FileTypeProperties::create (fileTypeInfo.id, fileTypeInfo.name, getSectionForType (fileTypeInfo.id)), -1, nullptr);
}
