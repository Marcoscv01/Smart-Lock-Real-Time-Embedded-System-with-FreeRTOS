# Cerrojo automático
Se hará el comportamiento lógico de un teclado matricial para poner la contraseña de un cerrojo automático. 
ada intento quedará registrado con hora y fecha. Además, se podrá abrir también haciendo uso de un RFID.

## Entradas/Salidas
Teclado matriial: Filas : 13, 12, 14, 27; Columnas: 26, 25, 33, 32
Servo Cerradura: GPIO15, con apertura en 2000 y cierre en 1000


# Teclado.h
Implementa la lectura de un teclado matricial 4×4 mediante escaneo periódico de filas y columnas usando GPIO.
Las filas se configuran como salidas y las columnas como entradas con pull-up. La detección de pulsaciones se realiza por nivel bajo, con debounce simple por retardo. Cada tecla detectada se envía a una cola FreeRTOS (cola_teclas).

Las teclas * y # son especiales:
*:Borrará el último dígito
#:Confirmación de la contraseña a mandar

# FSM
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

# Orden prioridades tareas
- tarea_teclado -->	6 (Escanea el hardware y genera eventos; si se retrasa mucho, podrías perder pulsaciones. Debe ser la más rápida)
- teclado_event_task -->5 (Toma las teclas de la cola y las convierte en eventos FSM. Necesita ser rápida para no bloquear la cola de teclas)
- fsm_task -->4	(Procesa la lógica de control. Puede esperar un poco si hay picos de entradas, porque los eventos se encolan en q_events)