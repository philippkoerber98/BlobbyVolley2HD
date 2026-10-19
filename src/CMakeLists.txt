set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
	add_compile_options(/utf-8)
	# When building for MSVC with CMake,
	# the compiler warning flags (like /W3) are added by default.
else()
	add_compile_options(-Wall)
endif()

include_directories(.)

set(BLOBBY_COMMON_SOURCES
	base64.cpp base64.h
	BlobbyDebug.cpp BlobbyDebug.h
	Clock.cpp Clock.h
	DuelMatch.cpp DuelMatch.h
	FileRead.cpp FileRead.h
	FileSystem.cpp FileSystem.h
	FileWrite.cpp FileWrite.h
	File.cpp File.h
	GameLogic.cpp GameLogic.h
	GenericIO.cpp GenericIO.h
	Global.h
	Color.cpp Color.h
	NetworkMessage.cpp NetworkMessage.h
	PhysicWorld.cpp PhysicWorld.h
	SpeedController.cpp SpeedController.h
	UserConfig.cpp UserConfig.h
	PhysicState.cpp PhysicState.h
	DuelMatchState.cpp DuelMatchState.h
	GameLogicState.cpp GameLogicState.h
	InputSource.cpp InputSource.h
	PlayerInput.h PlayerInput.cpp
	IScriptableComponent.cpp IScriptableComponent.h
	PlayerIdentity.cpp PlayerIdentity.h
	server/DedicatedServer.cpp server/DedicatedServer.h
	server/NetworkPlayer.cpp server/NetworkPlayer.h
	server/NetworkGame.cpp server/NetworkGame.h
	server/MatchMaker.cpp server/MatchMaker.h
	replays/ReplayRecorder.cpp replays/ReplayRecorder.h
	replays/ReplaySavePoint.cpp replays/ReplaySavePoint.h
	)

set (BLOBBY_CLIENT_SOURCES ${BLOBBY_COMMON_SOURCES}
	ApplicationLoop.cpp ApplicationLoop.h
	LevelCatalog.cpp LevelCatalog.h
	Blood.cpp Blood.h
	TextManager.cpp TextManager.h
	IMGUI.cpp IMGUI.h
	InputDevice.h
	InputManager.cpp InputManager.h
	LocalInputSource.cpp LocalInputSource.h
	RenderManager.cpp RenderManager.h
	RenderManagerText.cpp
	RenderManagerAssets.cpp
	RuntimeFont.cpp RuntimeFont.h
	UITranslations.cpp UITranslations.h
	RenderManagerGL2D.cpp RenderManagerGL2D.h
	RenderManagerSDL.cpp RenderManagerSDL.h
	RenderManagerNull.cpp RenderManagerNull.h
	ScriptedInputSource.cpp ScriptedInputSource.h
	SoundManager.cpp SoundManager.h
	Vector.h
	replays/ReplayPlayer.cpp replays/ReplayPlayer.h
	replays/ReplayLoader.cpp
	state/State.cpp state/State.h
	state/GameState.cpp state/GameState.h
	state/LocalGameState.cpp state/LocalGameState.h
	state/NetworkState.cpp state/NetworkState.h
	state/OptionsState.h
	state/TabbedOptionsState.cpp
	state/OptionsMenu.cpp state/OptionsMenu.h state/OptionsTabs.cpp
	state/OptionsWidgets.cpp
	state/NetworkSearchState.cpp state/NetworkSearchState.h
	state/ReplayState.cpp state/ReplayState.h
	state/ReplaySelectionState.cpp state/ReplaySelectionState.h
	state/LobbyStates.cpp state/LobbyStates.h
	input_device/JoystickInput.cpp
	input_device/JoystickPool.cpp input_device/JoystickPool.h
	input_device/KeyboardInput.cpp
	input_device/MouseInput.cpp
	input_device/TouchInput.cpp
	BlobbyApp.h BlobbyApp.cpp
	)

set (BLOBBY_SERVER_SOURCES ${BLOBBY_COMMON_SOURCES}
	server/servermain.cpp
	)

find_package(Boost REQUIRED)
find_package(OpenGL)
add_subdirectory(raknet)
add_subdirectory(blobnet)

if (OPENGL_FOUND)
	include_directories(${OPENGL_INCLUDE_DIR})
	add_definitions(-DHAVE_LIBGL)
endif (OPENGL_FOUND)

if (UNIX AND (NOT ANDROID))
	add_definitions("-DBLOBBY_DATA_DIR=\"${CMAKE_INSTALL_PREFIX}/share/blobby\"")
endif()

find_package(SDL2_image CONFIG REQUIRED)
find_package(SDL2_mixer CONFIG REQUIRED)
if(TARGET SDL2_mixer::SDL2_mixer)
    set(BLOBBY_AUDIO_LIBRARY SDL2_mixer::SDL2_mixer)
else()
    set(BLOBBY_AUDIO_LIBRARY SDL2_mixer::SDL2_mixer-static)
endif()
set(BLOBBY_COMMON_LIBS lua::lua blobnet::blobnet tinyxml2::tinyxml2 PhysFS::PhysFS SDL2::SDL2main SDL2::SDL2 SDL2_image::SDL2_image Boost::boost)

# other additional dependencies
if (NOT WIN32)
	find_package(Threads REQUIRED)
	set(BLOBBY_COMMON_LIBS ${BLOBBY_COMMON_LIBS} Threads::Threads)
endif ()

if (ANDROID)
    add_library(blobby SHARED main.cpp ${BLOBBY_CLIENT_SOURCES})
elseif (WIN32)
    add_executable(blobby WIN32 main.cpp ${BLOBBY_CLIENT_SOURCES})
else ()
    add_executable(blobby main.cpp ${BLOBBY_CLIENT_SOURCES})
endif ()

target_link_libraries(blobby ${BLOBBY_COMMON_LIBS} ${BLOBBY_AUDIO_LIBRARY} ${OPENGL_LIBRARIES})

add_executable(assets-smoke-test EXCLUDE_FROM_ALL ../test/AssetsSmokeTest.cpp ../test/FrameTimingTest.cpp ${BLOBBY_CLIENT_SOURCES})
target_sources(assets-smoke-test PRIVATE ../test/LevelAudioTest.cpp ../test/LANTest.cpp)
target_compile_definitions(assets-smoke-test PRIVATE BLOBBY_TEST_SOURCE_DIR="${PROJECT_SOURCE_DIR}/test")
target_link_libraries(assets-smoke-test PRIVATE ${BLOBBY_COMMON_LIBS} ${BLOBBY_AUDIO_LIBRARY} ${OPENGL_LIBRARIES})


if (UNIX AND (NOT ANDROID) OR WIN32)
	add_executable(blobby-server ${BLOBBY_SERVER_SOURCES})
	target_link_libraries(blobby-server ${BLOBBY_COMMON_LIBS})
endif ()
if (UNIX AND (NOT ANDROID))
	add_executable(blobby-runtest EXCLUDE_FROM_ALL runtest.cpp ${BLOBBY_CLIENT_SOURCES})
	target_link_libraries(blobby-runtest ${BLOBBY_COMMON_LIBS} ${BLOBBY_AUDIO_LIBRARY} ${OPENGL_LIBRARIES})

	add_executable(botbench EXCLUDE_FROM_ALL botbench.cpp ${BLOBBY_CLIENT_SOURCES})
	target_link_libraries(botbench ${BLOBBY_COMMON_LIBS} ${BLOBBY_AUDIO_LIBRARY} ${OPENGL_LIBRARIES})
endif ()

if (MSYS)
	set_target_properties(blobby PROPERTIES LINK_FLAGS "-mwindows") # disable the console window
	set_target_properties(blobby-server PROPERTIES LINK_FLAGS "-mconsole") # enable the console window
endif()

if (WIN32)
	install(TARGETS blobby blobby-server DESTINATION .)
elseif (ANDROID)
	install(TARGETS blobby DESTINATION bin)
elseif (UNIX AND NOT APPLE)
	install(TARGETS blobby blobby-server DESTINATION bin)
endif ()

if (MSVC)
	# set application icon
	target_sources(blobby PRIVATE ../win/res.rc)
	target_sources(blobby-server PRIVATE ../win/res.rc)
endif(MSVC)

# Header Checks
# Create a test target, consisting of one cpp file for each header, where the cpp file does nothing but
# include its header. If this compiles, then all our headers are self-contained.

# gather up the header files
file(GLOB_RECURSE HEADERS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.h)

# create a new target for this test. We use a static library (instead of an executable), because then
# we don't have to worry about things actually linking.
add_library(test-compile-headers STATIC EXCLUDE_FROM_ALL)

foreach(HEADER ${HEADERS})
	# construct a new file name based on the headers name
	string(MAKE_C_IDENTIFIER "${HEADER}" HEADER_TEST_FILE)
	set(TEST_COMPILE_FILE "${CMAKE_CURRENT_BINARY_DIR}/test_compile/${HEADER_TEST_FILE}.cpp")

	# create the test file and add to the target sources
	configure_file(test_compile.cpp.in ${TEST_COMPILE_FILE} @ONLY)
	target_sources(test-compile-headers PUBLIC ${TEST_COMPILE_FILE})
endforeach()

target_link_libraries(test-compile-headers PRIVATE ${BLOBBY_COMMON_LIBS} ${BLOBBY_AUDIO_LIBRARY})

add_executable(display-viewport-test EXCLUDE_FROM_ALL ../test/DisplayViewportTest.cpp)
target_link_libraries(display-viewport-test PRIVATE SDL2::SDL2main SDL2::SDL2)


if(SDL2_VERSION VERSION_GREATER_EQUAL "2.0.14")
	add_executable(controller-test EXCLUDE_FROM_ALL ../test/ControllerTest.cpp
		input_device/JoystickPool.cpp BlobbyDebug.cpp)
	target_link_libraries(controller-test PRIVATE SDL2::SDL2main SDL2::SDL2)
endif()

add_executable(tournament-test EXCLUDE_FROM_ALL ../test/TournamentTest.cpp server/Tournament.cpp BlobbyDebug.cpp)
target_link_libraries(tournament-test PRIVATE tinyxml2::tinyxml2 raknet::raknet)
