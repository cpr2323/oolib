#pragma once

#include <JuceHeader.h>
#include "DirectoryDataProperties.h"
#include "LambdaThread.h"
#include "ValueTreeMonitor.h"
#include "WatchDogTimer.h"

/*
    DirectoryValueTree scans a folder into a ValueTree, and keeps it up to date.

    It knows nothing about the kinds of files an application cares about. Clients describe those by
    registering file types, each of which is a name plus up to three callbacks:

        predicate  - identifies a file as being of this type
        decorator  - adds type specific properties to an entry (only run during a full scan)
        comparator - orders entries within this type's section of a folder listing

    registerFileType returns the id that will appear as the 'type' property of matching FileProperties
    entries. Two ids exist without being registered: unknownTypeId, used when no predicate matches, and
    folderTypeId, used for folders, which are identified structurally rather than by a predicate.

    Registered types are published into the DirectoryDataProperties tree, so that any client holding
    that tree can look an id up by name (DirectoryDataProperties::getFileTypeId) without the registering
    code having to hand the value around.

    Expected call order:

        init ()
        registerFileType () ...      // once per type, in the order the predicates should be tried
        setSortOrder ()              // optional
        setComparatorForType () ...  // optional, and the only way to reach the built-in types
        triggerStartScan ()          // via DirectoryDataProperties

    Registering after a scan has begun is not supported, as it would resize the section list out from
    under a running sort.

    THREADING: predicates, decorators and comparators are all called on the scan thread, never on the
    message thread. Anything they touch must be safe to use from there.
*/
class DirectoryValueTree : public juce::Thread,
                           private juce::Timer
{
public:
    DirectoryValueTree ();
    ~DirectoryValueTree ();

    // returns true if the file is of the type being registered
    using FileTypePredicate = std::function<bool (juce::File file)>;
    // adds type specific properties to a directory entry. only called during a full scan
    using EntryDecorator = std::function<void (juce::ValueTree entryVT, juce::File file)>;
    // returns true if firstName sorts before secondName. the names are as stored in the entry, ie. full paths
    using EntryComparator = std::function<bool (juce::String firstName, juce::String secondName)>;

    // ids that exist without being registered
    static constexpr int unknownTypeId { 0 };
    static constexpr int folderTypeId  { 1 };

    void init (juce::ValueTree rootPropertiesVT);
    juce::ValueTree getDirectoryDataPropertiesVT ();

    // registers a file type and returns its id. predicates are tried in registration order, so register
    // cheap tests before expensive ones. must be called after init (), and before the first scan
    int registerFileType (juce::String typeName, FileTypePredicate predicate,
                          EntryDecorator decorator = nullptr, EntryComparator comparator = nullptr);
    // attaches a comparator to an already known type. this is how the built-in folder and unknown
    // sections get one, since they are never registered
    void setComparatorForType (int typeId, EntryComparator comparator);
    // the order in which type sections appear in a sorted folder. must name every known id exactly once.
    // without it, the order is folders, then registered types in registration order, then unknown
    void setSortOrder (std::vector<int> newSortOrder);

private:
    enum class ScanType
    {
        checkForUpdate,
        fullScan
    };
    enum class TaskManagementState
    {
        idle,
        startScan,
        scanning,
        startCheck,
        checking,
    };

    struct FileTypeInfo
    {
        int id {};
        juce::String name;
        FileTypePredicate predicate;
        EntryDecorator decorator;
        EntryComparator comparator;
    };

    WatchdogTimer timer; // TODO - remove when not needed, ie. when done measuring things
    DirectoryDataProperties directoryDataProperties;
    LambdaThread scanThread { "ScanThread", 1000 };
    LambdaThread checkThread { "CheckThread", 1000 };

    // indexed by type id, so fileTypes [0] is unknown and fileTypes [1] is folder. written during
    // registration on the message thread, read by the scan/check threads once scanning has begun
    std::vector<FileTypeInfo> fileTypes;
    // sortOrder [section] is the type id displayed in that section, sectionForTypeId is its inverse
    std::vector<int> sortOrder;
    std::vector<int> sectionForTypeId;
    bool initialised { false };
    // registering or reordering after the first scan would resize the section list under a running sort
    bool scanEverStarted { false };

    int scanDepth { -1 };
    juce::int64 lastScanInProgressUpdate {};
    juce::CriticalSection rootFolderNameCS;
    juce::String rootFolderTaskName;     // written on the message thread (startScan), read by the scan/check threads
    juce::ValueTree rootFolderVTForTask; // handle to the live root folder tree, captured on the message thread in init,
                                         // only used to hand the live tree to message thread publish operations
    juce::ValueTree lastScanResultVT;    // detached copy of the last scan result, only accessed by the scan/check threads
    std::atomic<bool> cancelScan { false };
    std::atomic<bool> cancelCheck { false };
    // set when a scan is asked for, cleared when the scan thread is actually woken up. it keeps a task that is
    // reporting its own completion (ie. 'I am now idle') from overwriting a scan request that arrived while it was running
    std::atomic<bool> scanRequestPending { false };
    juce::CriticalSection taskManagementCS;
    TaskManagementState requestedTaskManagementState { TaskManagementState::idle };
    TaskManagementState currentTaskManagementState { TaskManagementState::idle };
    std::atomic<ScanType> scanType { ScanType::fullScan };

    void doIfProgressTimeElapsed (std::function<void ()> functionToDo);
    void doProgressUpdate (juce::String progressString);
    void getContentsOfFolder (juce::ValueTree folderVT, int curDepth, std::function<bool ()> shouldCancelFunc);
    juce::String getPathFromCurrentRoot (juce::String fullPath);
    juce::String getRootFolderTaskName ();
    TaskManagementState getCurrentTaskManagementState ();
    juce::String getTaskManagementStateString (TaskManagementState theThreadState);
    TaskManagementState getRequestedTaskManagementState ();
    bool hasFolderChanged ();
    juce::ValueTree makeFileEntry (juce::File file, juce::int64 createTime, juce::int64 modificationTime, int fileType);
    void scanDirectory ();
    void sendStatusUpdate (DirectoryDataProperties::ScanStatus scanStatus);
    void setCurrentTaskManagementState (DirectoryValueTree::TaskManagementState newThreadState);
    void setScanDepth (int theScanDepth);
    bool setRequestedTaskManagementState (DirectoryValueTree::TaskManagementState newThreadState);
    void setTaskCompleteIfNoScanPending ();
    bool shouldCancelOperation (LambdaThread& whichTaskThread, std::atomic<bool>& whichTaskCancelToCheck);
    void sortContentsOfFolder (juce::ValueTree rootFolderVT, std::function<bool ()> shouldCancelFunc);
    void startScan ();
    void wakeUpTaskManagmentThread ();

    // file type registry
    int identifyFileType (juce::File file);
    const FileTypeInfo* getFileTypeInfo (int typeId) const;
    int getSectionForType (int typeId) const;
    bool compareEntryNames (int typeId, juce::String firstName, juce::String secondName) const;
    void applySortOrder (std::vector<int> newSortOrder);
    void useDefaultSortOrderIfUnset ();
    void publishFileTypes ();

    ValueTreeMonitor ddpMonitor;
    ValueTreeMonitor rootFolderMonitor;

    void run () override;
    void timerCallback () override;
};
