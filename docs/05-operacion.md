# Operación y recuperación

## Objetivos

| Indicador             |        Objetivo |
| --------------------- | --------------: |
| Lectura disponible    |         99,95 % |
| Escritura disponible  |         99,90 % |
| p95 de comando        |        < 750 ms |
| Conciliación conforme | 100 % por época |
| Retraso de diario     |      < 2 épocas |

La conservación prevalece sobre disponibilidad. Si falta versión o conciliación, se mantiene lectura y se cierra escritura.

```mermaid
flowchart TD
    TAG["Etiqueta aprobada"] --> VERIFY["Commit y digest"]
    VERIFY --> CANARY["Canaria sin escritura"]
    CANARY --> SHADOW["Conciliación paralela"]
    SHADOW --> LIMITED["Escritura limitada"]
    LIMITED --> FULL["Promoción gradual"]
    FULL --> WATCH["Observación"]
    VERIFY -->|Diferencia| STOP["Detener"]
    SHADOW -->|Diferencia| STOP
    LIMITED -->|Señal material| ROLLBACK["Cerrar y revertir binario"]
```

## Copias

Conservar snapshot, eventos posteriores, tickets, idempotencias, configuración y manifiesto de hashes. La frecuencia debe limitar la pérdida de datos al objetivo acordado.

```mermaid
sequenceDiagram
    autonumber
    participant O as Operador
    participant B as Copias
    participant N as Instancia nueva
    participant R as Conciliador
    O->>B: Elegir manifiesto verificado
    B-->>N: Snapshot, eventos y tickets
    N->>N: Reproducir secuencia
    N->>R: Exponer lectura
    R->>R: Comparar reservas y créditos
    R-->>O: Informe firmado
    O->>N: Apertura gradual
```

## Intervenciones

### Diferencia de reserva

Cerrar salidas del activo, preservar fuentes, distinguir valoración de cantidad, localizar primera secuencia y corregir con asiento compensatorio aprobado.

### Lote detenido

Agrupar tickets por estado y época, revisar pago mínimo, límite, liquidez y versión. No recrear tickets con otro identificador.

### Presión de capital

Congelar expansión, revisar recortes y salidas, recapitalizar por segregación y mantener dos ventanas conformes antes de restaurar cuotas.

## Mantenimiento

- semanal: colas, espacio, identidades y dependencias;
- mensual: restauración completa;
- trimestral: cancelación de guardián y pérdida de zona;
- por versión: migración, reversión y comparación de contratos JSON.
