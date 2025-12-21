# Cerrojo automático
Se hará el comportamiento lógico de un teclado matricial para poner la contraseña de un cerrojo automático. 
ada intento quedará registrado con hora y fecha. Además, se podrá abrir también haciendo uso de un RFID.

## Entradas/Salidas
Teclado matriial: Filas : 13, 12, 14, 27; Columnas: 26, 25, 33, 32



# Teclado.h
Implementa la lectura de un teclado matricial 4×4 mediante escaneo periódico de filas y columnas usando GPIO.
Las filas se configuran como salidas y las columnas como entradas con pull-up. La detección de pulsaciones se realiza por nivel bajo, con debounce simple por retardo. Cada tecla detectada se envía a una cola FreeRTOS (cola_teclas).

Las teclas * y # son especiales:
*:Borrará el último dígito
#:Confirmación de la contraseña a mandar