#pragma once

#include <JuceHeader.h>
#include "ValueTreeWrapper.h"

class DirectoryDataProperties : public ValueTreeWrapper<DirectoryDataProperties>
{
public:
    DirectoryDataProperties () noexcept : ValueTreeWrapper<DirectoryDataProperties> (DirectoryDataTypeId)
    {
    }
    DirectoryDataProperties (juce::ValueTree vt, WrapperType wrapperType, EnableCallbacks shouldEnableCallbacks) noexcept
        : ValueTreeWrapper<DirectoryDataProperties> (DirectoryDataTypeId, vt, wrapperType, shouldEnableCallbacks)
    {
    }

    enum class ScanStatus
    {
        empty,
        scanning,
        canceled,
        done
    };

    void setProgress (juce::String progressString, bool includeSelfCallback);
    void setRootFolder (juce::String rootFolder, bool includeSelfCallback);
    void setScanDepth (int scanDepth, bool includeSelfCallback);
    void setStatus (ScanStatus status, bool includeSelfCallback);
    void triggerRootScanComplete (bool includeSelfCallback);
    void triggerStartScan (bool includeSelfCallback);

    juce::String getProgress ();
    juce::String getRootFolder ();
    int getScanDepth ();
    DirectoryDataProperties::ScanStatus getStatus ();

    std::function<void (juce::String progressString)> onProgressChange;
    std::function<void (juce::String rootFolder)> onRootFolderChange;
    std::function<void ()> onRootScanComplete;
    std::function<void (int scanDepth)> onScanDepthChange;
    std::function<void (ScanStatus status)> onStatusChange;
    std::function<void ()> onStartScanChange;

    juce::ValueTree getRootFolderVT ();

    /*
        The registry of file types, published by DirectoryValueTree as types are registered.
        Clients read it to turn a type name into the id they will see on a FileProperties entry,
        which is what replaces the old compile time TypeIndex enum.
    */
    juce::ValueTree getFileTypesVT ();
    int getFileTypeId (juce::String typeName);
    void forEachFileType (std::function<bool (int typeId, juce::String typeName)> fileTypeCallback);

    static inline const juce::Identifier DirectoryDataTypeId { "DirectoryData" };
    static inline const juce::Identifier ProgressPropertyId   { "progress" };
    static inline const juce::Identifier RootFolderPropertyId { "rootFolder" };
    static inline const juce::Identifier RootScanCompletePropertyId { "rootScanComplete" };
    static inline const juce::Identifier ScanDepthPropertyId  { "scanDepth" };
    static inline const juce::Identifier StartScanPropertyId  { "startScan" };
    static inline const juce::Identifier StatusPropertyId     { "status" };

    static inline const juce::Identifier DirectoryValueTreeTypeId { "directoryValueTree" };
    static inline const juce::Identifier FileTypesTypeId { "FileTypes" };

    void initValueTree ();
    void processValueTree () {}

private:
    void valueTreePropertyChanged (juce::ValueTree& vt, const juce::Identifier& property) override;
};

// one entry in the file type registry. written by DirectoryValueTree, read by clients
class FileTypeProperties : public ValueTreeWrapper<FileTypeProperties>
{
public:
    FileTypeProperties () noexcept : ValueTreeWrapper<FileTypeProperties> (FileTypeTypeId)
    {
    }
    FileTypeProperties (juce::ValueTree vt, WrapperType wrapperType, EnableCallbacks shouldEnableCallbacks) noexcept
        : ValueTreeWrapper<FileTypeProperties> (FileTypeTypeId, vt, wrapperType, shouldEnableCallbacks)
    {
    }

    void setId (int theId, bool includeSelfCallback)
    {
        setValue (theId, IdPropertyId, includeSelfCallback);
    }

    int getId ()
    {
        return getValue<int> (IdPropertyId);
    }

    void setName (juce::String theName, bool includeSelfCallback)
    {
        setValue (theName, NamePropertyId, includeSelfCallback);
    }

    juce::String getName ()
    {
        return getValue<juce::String> (NamePropertyId);
    }

    // the section this type occupies in the sorted folder listing. -1 until a sort order is established
    void setSortPosition (int sortPosition, bool includeSelfCallback)
    {
        setValue (sortPosition, SortPositionPropertyId, includeSelfCallback);
    }

    int getSortPosition ()
    {
        return getValue<int> (SortPositionPropertyId);
    }

    static inline const juce::Identifier FileTypeTypeId { "FileType" };
    static inline const juce::Identifier IdPropertyId           { "id" };
    static inline const juce::Identifier NamePropertyId         { "name" };
    static inline const juce::Identifier SortPositionPropertyId { "sortPosition" };

    static juce::ValueTree create (int typeId, juce::String typeName, int sortPosition)
    {
        juce::ValueTree fileTypeVT { FileTypeTypeId };
        fileTypeVT.setProperty (IdPropertyId, typeId, nullptr);
        fileTypeVT.setProperty (NamePropertyId, typeName, nullptr);
        fileTypeVT.setProperty (SortPositionPropertyId, sortPosition, nullptr);
        return fileTypeVT;
    }

    void initValueTree () {}
    void processValueTree () {}

private:
};

class FileProperties : public ValueTreeWrapper<FileProperties>
{
public:
    FileProperties () noexcept : ValueTreeWrapper<FileProperties> (FileTypeId)
    {
    }
    FileProperties (juce::ValueTree vt, WrapperType wrapperType, EnableCallbacks shouldEnableCallbacks) noexcept
        : ValueTreeWrapper<FileProperties> (FileTypeId, vt, wrapperType, shouldEnableCallbacks)
    {
    }

    void setName (juce::String name, bool includeSelfCallback)
    {
        setValue (name, NamePropertyId, includeSelfCallback);
    }

    juce::String getName ()
    {
        return getValue<juce::String> (NamePropertyId);
    }

    // the file type is an id handed out by DirectoryValueTree::registerFileType, or one of its
    // built-in ids (DirectoryValueTree::unknownTypeId). this class attaches no meaning to the value
    void setType (int theType, bool includeSelfCallback)
    {
        setValue (theType, TypePropertyId, includeSelfCallback);
    }

    int getType ()
    {
        return getValue<int> (TypePropertyId);
    }

    void setCreateTime (juce::int64 time, bool includeSelfCallback)
    {
        setValue (time, CreationTimePropertyId, includeSelfCallback);
    }

    juce::int64 getCreateTime ()
    {
        return getValue<juce::int64> (CreationTimePropertyId);
    }

    void setModificationTime (juce::int64 time, bool includeSelfCallback)
    {
        setValue (time, ModificationTimePropertyId, includeSelfCallback);
    }

    juce::int64 getModificationTime ()
    {
        return getValue<juce::int64> (ModificationTimePropertyId);
    }

    static inline const juce::Identifier FileTypeId { "File" };
    static inline const juce::Identifier NamePropertyId             { "name" };
    static inline const juce::Identifier TypePropertyId             { "type" };
    static inline const juce::Identifier CreationTimePropertyId     { "createTime" };
    static inline const juce::Identifier ModificationTimePropertyId { "modificationTime" };

    static juce::ValueTree create (juce::String filePath, juce::int64 createTime, juce::int64 modificationTime, int fileType)
    {
        juce::ValueTree fileVT { FileTypeId };
        fileVT.setProperty (NamePropertyId, filePath, nullptr);
        fileVT.setProperty (TypePropertyId, fileType, nullptr);
        fileVT.setProperty (CreationTimePropertyId, createTime, nullptr);
        fileVT.setProperty (ModificationTimePropertyId, modificationTime, nullptr);
        return fileVT;
    }

    static bool isFileVT (juce::ValueTree directoryEntryVT)
    {
        return directoryEntryVT.getType () == FileTypeId;
    }

    void initValueTree () {}
    void processValueTree () {}

private:
};

class FolderProperties : public ValueTreeWrapper<FolderProperties>
{
public:
    FolderProperties () noexcept : ValueTreeWrapper<FolderProperties> (FolderTypeId)
    {
    }
    FolderProperties (juce::ValueTree vt, WrapperType wrapperType, EnableCallbacks shouldEnableCallbacks) noexcept
        : ValueTreeWrapper<FolderProperties> (FolderTypeId, vt, wrapperType, shouldEnableCallbacks)
    {
    }

    void setName (juce::String name, bool includeSelfCallback)
    {
        setValue (name, NamePropertyId, includeSelfCallback);
    }

    juce::String getName ()
    {
        return getValue<juce::String> (NamePropertyId);
    }

    void setCreateTime (juce::int64 time, bool includeSelfCallback)
    {
        setValue (time, CreationTimePropertyId, includeSelfCallback);
    }

    juce::int64 getCreateTime ()
    {
        return getValue<juce::int64> (CreationTimePropertyId);
    }

    void setModificationTime (juce::int64 time, bool includeSelfCallback)
    {
        setValue (time, ModificationTimePropertyId, includeSelfCallback);
    }

    juce::int64 getModificationTime ()
    {
        return getValue<juce::int64> (ModificationTimePropertyId);
    }

    std::function<void (juce::ValueTree folder)> onFolderAdded;
    std::function<void (juce::ValueTree folder)> onFolderRemoved;
    std::function<void (juce::ValueTree folder)> onFolderUpdated;
    std::function<void (juce::ValueTree file)> onFileAdded;
    std::function<void (juce::ValueTree file)> onFileRemoved;
    std::function<void (juce::ValueTree file)> onFileUpdated;

    static inline const juce::Identifier FolderTypeId { "Folder" };
    static inline const juce::Identifier NamePropertyId             { "name" };
    static inline const juce::Identifier StatusPropertyId           { "status" };
    static inline const juce::Identifier CreationTimePropertyId     { "createTime" };
    static inline const juce::Identifier ModificationTimePropertyId { "modificationTime" };

    static bool isFolderVT (juce::ValueTree directoryEntryVT)
    {
        return directoryEntryVT.getType () == FolderTypeId;
    }

    static juce::ValueTree create (juce::String filePath, juce::int64 createTime, juce::int64 modificationTime)
    {
        juce::ValueTree fileVT { FolderTypeId };
        fileVT.setProperty (NamePropertyId, filePath, nullptr);
        fileVT.setProperty (StatusPropertyId, "unscanned", nullptr);
        fileVT.setProperty (CreationTimePropertyId, createTime, nullptr);
        fileVT.setProperty (ModificationTimePropertyId, modificationTime, nullptr);
        return fileVT;
    }

    void initValueTree () {}
    void processValueTree () {}

private:
};
