# Arquitectura del sistema

## 1. Entradas/salidas
- Teclado matriial: Filas : 13, 12, 14, 27; Columnas: 26, 25, 33, 32
- Servo Cerradura: GPIO15, con apertura en 2000 y cierre en 1000
- Boton Panico: GPIO0, el BOOT del ESP, simulando una emergencia que necesite tener la apertura de la cerradura forzada
- I2C: SCL 18, SDA 19 (direcciones 0x68 LCD y 0x27 RTC)
- SPI lector SD: MOSI 22; MISO 23; SCK 4; CS 2


## 2. Tareas, periodos y prioridades

| Tarea        | Función principal                  | Periodo / Activación    | Prioridad |
|-------------|-----------------------------------|--------------------------|-----------|
| panic       | Gestión de emergencia (ISR→FSM)   | Evento (GPIO ISR)         | 7 |
| fsm         | Lógica principal del sistema       | Evento (cola)            | 6 |
| servo       | Control del actuador               | Evento FSM               | 5 |
| kbd_evt     | Traducción teclas → eventos FSM    | Evento (cola)            | 4 |
| kbd_scan    | Escaneo periódico teclado          | 50 ms                    | 3 |
| lcd         | Interfaz de usuario                | 500 ms                   | 2 |
| rtc         | Lectura RTC por I2C                | 200 ms                   | 2 |
| sd          | Logging de eventos                 | Evento FSM               | 1 |
| monitor     | Monitorización y métricas          | 2000 ms                  | 1 |

## 3. Recursos compartidos

- `mutex_i2c`: protege el bus I2C (RTC + LCD)
- `mutex_time`: protege la estructura `rtc_time`
- `mutex_lcd`: protege el buffer de texto del LCD
- `sem_panic`: señalización ISR → task panic

## 4. Colas

| Cola        | Productor | Consumidor | Uso |
|------------|-----------|------------|-----|
| cola_teclas | kbd_scan | kbd_evt | Teclas crudas |
| q_events | varias | fsm | Eventos FSM |
| q_fsm_lcd | fsm | lcd | Estado a mostrar |
| q_fsm_servo | fsm | servo | Orden actuador |
| q_fsm_sd | fsm | sd | Eventos a log |

## 5. Diagrama lógico (resumen)

ISR → panic_task → FSM  
Teclado → FSM → {LCD, SERVO, SD}  
RTC → mutex_time → LCD  
Monitor observa sin interferir

La maquina de estados funciona de la siguiente manera:
      +---------+
      | ST_INIT | <--- EVT_STARTUP
      +----+----+
           |
           v
      +----+----+
      |  ST_IDLE| <--- EVT_ENTER_IDLE
      +----+----+
           |
           v
 +-------------------+
 | ST_ENTRADA_PIN     |
 | - EVT_TECLA_DIGITO |
 | - EVT_TECLA_BORRAR |
 | - EVT_TECLA_CONFIRMAR --> PIN correcto --> ST_ABIERTO
 |                        \
 |                         --> PIN incorrecto --> ST_ERROR
 +-------------------+
           |
           v
      +----+----+
      | ST_ABIERTO|
      +----+----+
           |
           v
      +----+----+
      |  ST_IDLE |  (después de cerrar servo)
      +----+----+
           ^
           |
      +----+----+
      | ST_ERROR |
      +----+----+
           |
           v
      +----+----+
      |  ST_IDLE |
      +---------+
