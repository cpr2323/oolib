# oolib - shared utility sources
#
# Usage from a consuming JUCE project:
#     include (submodules/oolib/oolib.cmake)
#     target_sources (MyApp PRIVATE ${OOLIB_SOURCES})
#     target_include_directories (MyApp PRIVATE ${OOLIB_INCLUDE_DIR})
#
# Sources include each other as "oolib/<Category>/<File>.h", so OOLIB_INCLUDE_DIR is the
# directory holding the oolib/ folder. Nothing in the source refers to where oolib is checked
# out, which means moving it only changes the line above.
#
# These files #include <JuceHeader.h>, so they must be compiled as part of a JUCE target
# rather than as a standalone library.

set (OOLIB_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR})

set (OOLIB_SOURCES
    # Core
    ${CMAKE_CURRENT_LIST_DIR}/oolib/Core/Crc.h
    ${CMAKE_CURRENT_LIST_DIR}/oolib/Core/LambdaThread.h
    # Debug
    ${CMAKE_CURRENT_LIST_DIR}/oolib/Debug/DebugLog.cpp
    ${CMAKE_CURRENT_LIST_DIR}/oolib/Debug/DebugLog.h
    ${CMAKE_CURRENT_LIST_DIR}/oolib/Debug/DebugLogImplementation.h
    ${CMAKE_CURRENT_LIST_DIR}/oolib/Debug/DumpStack.cpp
    ${CMAKE_CURRENT_LIST_DIR}/oolib/Debug/DumpStack.h
    ${CMAKE_CURRENT_LIST_DIR}/oolib/Debug/ValueTreeMonitor.cpp
    ${CMAKE_CURRENT_LIST_DIR}/oolib/Debug/ValueTreeMonitor.h
    ${CMAKE_CURRENT_LIST_DIR}/oolib/Debug/WatchDogTimer.h
    # Directory
    ${CMAKE_CURRENT_LIST_DIR}/oolib/Directory/DirectoryDataProperties.cpp
    ${CMAKE_CURRENT_LIST_DIR}/oolib/Directory/DirectoryDataProperties.h
    ${CMAKE_CURRENT_LIST_DIR}/oolib/Directory/DirectoryValueTree.cpp
    ${CMAKE_CURRENT_LIST_DIR}/oolib/Directory/DirectoryValueTree.h
    # GUI
    ${CMAKE_CURRENT_LIST_DIR}/oolib/GUI/CustomComboBox.cpp
    ${CMAKE_CURRENT_LIST_DIR}/oolib/GUI/CustomComboBox.h
    ${CMAKE_CURRENT_LIST_DIR}/oolib/GUI/CustomComponentMouseHandler.cpp
    ${CMAKE_CURRENT_LIST_DIR}/oolib/GUI/CustomComponentMouseHandler.h
    ${CMAKE_CURRENT_LIST_DIR}/oolib/GUI/CustomTextButton.cpp
    ${CMAKE_CURRENT_LIST_DIR}/oolib/GUI/CustomTextButton.h
    ${CMAKE_CURRENT_LIST_DIR}/oolib/GUI/CustomTextEditor.h
    ${CMAKE_CURRENT_LIST_DIR}/oolib/GUI/ErrorHelpers.cpp
    ${CMAKE_CURRENT_LIST_DIR}/oolib/GUI/ErrorHelpers.h
    ${CMAKE_CURRENT_LIST_DIR}/oolib/GUI/FileSelectLabel.cpp
    ${CMAKE_CURRENT_LIST_DIR}/oolib/GUI/FileSelectLabel.h
    ${CMAKE_CURRENT_LIST_DIR}/oolib/GUI/NoArrowComboBoxLnF.h
    ${CMAKE_CURRENT_LIST_DIR}/oolib/GUI/RoundedSlideSwitch.cpp
    ${CMAKE_CURRENT_LIST_DIR}/oolib/GUI/RoundedSlideSwitch.h
    ${CMAKE_CURRENT_LIST_DIR}/oolib/GUI/SplitWindowComponent.cpp
    ${CMAKE_CURRENT_LIST_DIR}/oolib/GUI/SplitWindowComponent.h
    # Properties
    ${CMAKE_CURRENT_LIST_DIR}/oolib/Properties/PersistentRootProperties.cpp
    ${CMAKE_CURRENT_LIST_DIR}/oolib/Properties/PersistentRootProperties.h
    ${CMAKE_CURRENT_LIST_DIR}/oolib/Properties/RootProperties.cpp
    ${CMAKE_CURRENT_LIST_DIR}/oolib/Properties/RootProperties.h
    ${CMAKE_CURRENT_LIST_DIR}/oolib/Properties/RuntimeRootProperties.cpp
    ${CMAKE_CURRENT_LIST_DIR}/oolib/Properties/RuntimeRootProperties.h
    # ValueTree
    ${CMAKE_CURRENT_LIST_DIR}/oolib/ValueTree/ValueTreeFile.cpp
    ${CMAKE_CURRENT_LIST_DIR}/oolib/ValueTree/ValueTreeFile.h
    ${CMAKE_CURRENT_LIST_DIR}/oolib/ValueTree/ValueTreeHelpers.cpp
    ${CMAKE_CURRENT_LIST_DIR}/oolib/ValueTree/ValueTreeHelpers.h
    ${CMAKE_CURRENT_LIST_DIR}/oolib/ValueTree/ValueTreeWrapper.cpp
    ${CMAKE_CURRENT_LIST_DIR}/oolib/ValueTree/ValueTreeWrapper.h
)
