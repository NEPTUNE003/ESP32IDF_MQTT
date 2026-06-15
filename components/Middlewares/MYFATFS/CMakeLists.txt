list(APPEND srcs    exfuns.c ff_utils.c)

idf_component_register(SRCS "exfuns.c" "ff_utils.c"
                    INCLUDE_DIRS "."
                    REQUIRES fatfs
                    )