idf_component_register(SRCS "main.c"
                            "../components/sx127x/examples/common/esp_utils/esp_utils.c"
                            "src/motionSensor.c"
                       INCLUDE_DIRS 
                              "include"
                              "."
                       PRIV_REQUIRES sx127x
                       PRIV_INCLUDE_DIRS "../components/sx127x/examples/common/esp_utils")