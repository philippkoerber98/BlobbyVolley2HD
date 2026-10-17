# Runtime graphics live directly in gfx. Other categories
# retain their source hierarchy (for example scripts/wide).
function(add_runtime_archive directory extension)
    if(directory STREQUAL "gfx")
        file(GLOB archive_sources CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
            gfx/*.png gfx/*.webp gfx/*.ttf gfx/*.txt)
    elseif(directory STREQUAL "sounds")
        file(GLOB_RECURSE archive_sources CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
            sounds/*.wav sounds/*.ogg)
    else()
        file(GLOB_RECURSE archive_sources CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
            ${directory}/*.${extension})
    endif()

    # Include exactly one version of each image, preferring WebP over PNG.
    foreach(asset IN LISTS archive_sources)
        if(asset MATCHES "\\.png$")
            string(REGEX REPLACE "\\.png$" ".webp" compressed_asset "${asset}")
            if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${compressed_asset}")
                list(REMOVE_ITEM archive_sources "${asset}")
            endif()
        endif()
    endforeach()

    set(archive "${CMAKE_CURRENT_BINARY_DIR}/${directory}.zip")
    add_custom_command(
        OUTPUT "${archive}"
        COMMAND ${CMAKE_COMMAND} -E tar cf "${archive}" --format=zip ${archive_sources}
        DEPENDS ${archive_sources}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        VERBATIM)
    add_custom_target(${directory}_zip ALL DEPENDS "${archive}")
endfunction()

add_runtime_archive(gfx webp)
add_runtime_archive(sounds wav)
add_runtime_archive(scripts lua)
add_runtime_archive(rules lua)

set(install_files
	${CMAKE_CURRENT_SOURCE_DIR}/ui-translations.xml
	${CMAKE_CURRENT_BINARY_DIR}/gfx.zip
	${CMAKE_CURRENT_BINARY_DIR}/sounds.zip
	${CMAKE_CURRENT_BINARY_DIR}/scripts.zip
	${CMAKE_CURRENT_BINARY_DIR}/rules.zip
	${CMAKE_CURRENT_SOURCE_DIR}/api.lua
	${CMAKE_CURRENT_SOURCE_DIR}/bot_api.lua
	${CMAKE_CURRENT_SOURCE_DIR}/rules_api.lua
	${CMAKE_CURRENT_SOURCE_DIR}/config.xml
	${CMAKE_CURRENT_SOURCE_DIR}/inputconfig.xml
	${CMAKE_CURRENT_SOURCE_DIR}/default-config.xml
	${CMAKE_CURRENT_SOURCE_DIR}/default-inputconfig.xml
	${CMAKE_CURRENT_SOURCE_DIR}/server.xml
	${CMAKE_CURRENT_SOURCE_DIR}/lang_cs.xml
	${CMAKE_CURRENT_SOURCE_DIR}/lang_de.xml
	${CMAKE_CURRENT_SOURCE_DIR}/lang_en.xml
	${CMAKE_CURRENT_SOURCE_DIR}/lang_es.xml
	${CMAKE_CURRENT_SOURCE_DIR}/lang_fr.xml
	${CMAKE_CURRENT_SOURCE_DIR}/lang_it.xml)

if (WIN32)
	install(FILES ${install_files} DESTINATION data)
elseif (UNIX AND NOT APPLE)
	install(FILES ${install_files} DESTINATION share/blobby)
endif (WIN32)

set(data_files ${install_files} PARENT_SCOPE)

# Level folders remain editable beside the Windows executable.
if(WIN32)
    install(DIRECTORY Levels "Custom Levels" DESTINATION .)
elseif(APPLE AND BUILD_MACOS_BUNDLE)
    install(DIRECTORY Levels DESTINATION blobby.app/Contents/Resources)
    install(DIRECTORY "Custom Levels" DESTINATION .)
else()
    install(DIRECTORY Levels "Custom Levels" DESTINATION share/blobby)
endif()
