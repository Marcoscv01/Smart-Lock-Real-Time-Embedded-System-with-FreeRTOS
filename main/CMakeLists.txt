idf_component_register(SRCS "fsm.c" "Teclado.c" "servo.c" "rtc.c" "lcd.c" "main.c"
                       PRIV_REQUIRES driver esp_timer
                       INCLUDE_DIRS "")
