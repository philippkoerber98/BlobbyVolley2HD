macro(add_zip_archive _dir _filetype)
	# Includes optional high-resolution images below gfx/hd.
	file(GLOB_RECURSE archive_src RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${_dir}/*.${_filetype})
	if("${_dir}" STREQUAL "gfx" OR "${_dir}" STREQUAL "backgrounds")
		file(GLOB_RECURSE png_src RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${_dir}/*.png)
		list(APPEND archive_src ${png_src})
	endif()
	if("${_dir}" STREQUAL "gfx")
		file(GLOB archive_src CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} gfx/hd/*.png gfx/hd/*.webp gfx/hd/*.txt gfx/hd/*.ttf)
	elseif("${_dir}" STREQUAL "backgrounds")
		file(GLOB archive_src CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} backgrounds/hd/*.png backgrounds/hd/*.webp)
	endif()
    # Package only the preferred format of each HD image, never both copies.
    if("${_dir}" STREQUAL "gfx" OR "${_dir}" STREQUAL "backgrounds")
        foreach(asset ${archive_src})
            if(asset MATCHES "\\.png$")
                string(REGEX REPLACE "\\.png$" ".webp" webp_asset "${asset}")
                if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${webp_asset}")
                    list(REMOVE_ITEM archive_src "${asset}")
                endif()
            endif()
        endforeach()
    endif()
	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_dir}.zip
		COMMAND ${CMAKE_COMMAND} -E tar cf ${CMAKE_CURRENT_BINARY_DIR}/${_dir}.zip --format=zip ${archive_src}
		DEPENDS ${archive_src}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		VERBATIM
		)
	add_custom_target(${_dir}_zip ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${_dir}.zip)
#	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${_dir].zip DESTINATION /usr)
endmacro(add_zip_archive _dir _filetype)


add_zip_archive(gfx bmp)
add_zip_archive(sounds wav)
add_zip_archive(scripts lua)
add_zip_archive(backgrounds bmp)
add_zip_archive(rules lua)

set(install_files
	${CMAKE_CURRENT_SOURCE_DIR}/ui-translations.xml
	${CMAKE_CURRENT_BINARY_DIR}/gfx.zip
	${CMAKE_CURRENT_BINARY_DIR}/sounds.zip
	${CMAKE_CURRENT_BINARY_DIR}/scripts.zip
	${CMAKE_CURRENT_BINARY_DIR}/backgrounds.zip
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
