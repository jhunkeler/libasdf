function(submodule_init)
    foreach (path IN LISTS ${ARGV0})
        string(REGEX REPLACE "[\ \\\/-]+" "_" target_name ${path})
        execute_process(
                COMMAND "git config get submodule.${path}.ignore"
                RESULT_VARIABLE ignored
        )
        #add_custom_target(init_submodule_${target_name}
        #        ALL
        #        COMMAND cd ${CMAKE_SOURCE_DIR} && git submodule update --init ${path}
        #)
        #add_custom_target(init_submodule_config_${target_name}
        #        ALL
        #        COMMAND cd ${CMAKE_SOURCE_DIR} && git config submodule.${path}.ignore untracked
        #        DEPENDS init_submodule_${target_name}
        #)

        if(NOT ignored)
            execute_process(
                    COMMAND cd ${CMAKE_SOURCE_DIR} && git submodule update --init ${path}
            )
            execute_process(
                    COMMAND cd ${CMAKE_SOURCE_DIR} && git config submodule.${path}.ignore untracked
            )
        endif()
    endforeach()
endfunction()
