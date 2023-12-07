idf_component_register(SRCS "sgp41.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver esp_timer i2c_bus)
