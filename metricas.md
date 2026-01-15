# Métricas del sistema

## 1. CPU por tarea (RunTimeStats)

- IDLE > 90%
- LCD ≈ 4%
- Resto de tareas < 1%

Conclusión: sistema sobredimensionado en CPU.

## 2. Latencias medidas

- FSM → SERVO: 30–200 µs
- FSM → SD: < 150 µs
- FSM → LCD: 90–380 ms

Causa principal LCD: periodo + acceso I2C + mutex.

## 3. Jitter

### RTC (200 ms)
- Jitter medio: bajo (≈ µs–ms)
- Jitter máximo: coincide con acceso LCD al I2C

### LCD (500 ms)
- Jitter visible pero tolerable
- No afecta a seguridad

## 4. Stacks y colas

- Sin overflow salvo monitor (ajustado)
- Colas de longitud 1 usadas como último estado
- Diseño intencionado para evitar backlog

## 5. Decisiones de diseño

- Prioridad alta a FSM y emergencia
- LCD y SD en prioridades bajas
- 
