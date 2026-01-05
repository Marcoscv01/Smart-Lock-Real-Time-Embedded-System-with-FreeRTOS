idf_component_register(SRCS "sd.c" "fsm.c" "Teclado.c" "servo.c" "rtc.c" "lcd.c" "main.c"
                       PRIV_REQUIRES driver esp_timer fatfs vfs 
                       INCLUDE_DIRS "")
