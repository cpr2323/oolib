# oolib - shared utility sources
#
# Usage from a consuming JUCE project:
#     include (submodules/oolib/oolib.cmake)
#     target_sources (MyApp PRIVATE ${OOLIB_SOURCES})
#     target_include_directories (MyApp PRIVATE ${OOLIB_INCLUDE_DIR})
#
# These files #include <JuceHeader.h>, so they must be compiled as part of a
# JUCE target rather than as a standalone library.

set (OOLIB_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/Source)

set (OOLIB_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/Source/Crc.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/CustomComboBox.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/CustomComboBox.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/CustomComponentMouseHandler.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/CustomComponentMouseHandler.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/CustomTextButton.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/CustomTextButton.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/CustomTextEditor.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/DebugLog.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/DebugLog.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/DebugLogImplementation.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/DumpStack.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/DumpStack.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/ErrorHelpers.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/ErrorHelpers.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/FileSelectLabel.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/FileSelectLabel.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/LambdaThread.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/MruListProperties.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/MruListProperties.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/NoArrowComboBoxLnF.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/PersistentRootProperties.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/PersistentRootProperties.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/RootProperties.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/RootProperties.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/RoundedSlideSwitch.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/RoundedSlideSwitch.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/RuntimeRootProperties.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/RuntimeRootProperties.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/SinglePoleFilter.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/SplitWindowComponent.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/SplitWindowComponent.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/ValueTreeFile.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/ValueTreeFile.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/ValueTreeHelpers.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/ValueTreeHelpers.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/ValueTreeMonitor.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/ValueTreeMonitor.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/ValueTreeWrapper.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/ValueTreeWrapper.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/WatchDogTimer.h
)
