idf_component_register(
                    SRCS "KeyFunction.c" "main.c"
                    "../RTC/RTC.c"
                    "../Tft/TftLcdSpi.c"
                    "../led/led_main.c"
                    "../mDNS/mdns_main.c"
                    "../FileServer/filemain.c"
                    "../FileServer/file_Server.c"
                    "../ota_main/ota_main.c"
                    "../uart/uart_main.c"
                    "../JsonSocket/JsonSocket.c"
                    "../NVS/nvsUserData.c"

                    INCLUDE_DIRS "."
                    
                    EMBED_FILES "../FileServer/favicon.ico" 
                    "../FileServer/upload_script.html" 
                    "../FileServer/upload_script_old.html" 
                    "../FileServer/demo_con.html" 
                    "../FileServer/demo_home.html"
)