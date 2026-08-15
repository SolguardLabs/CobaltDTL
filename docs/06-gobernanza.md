# Gobernanza

## Identidad

Cada operación liga protocolo, red, destino, método, hash de carga, predecesor, sal y época. Las cadenas se codifican con longitud antes de aplicar SHA-256, evitando ambigüedad por concatenación.

```mermaid
flowchart LR
    P["Protocolo"] --> C["Codificación canónica"]
    N["Red"] --> C
    T["Destino y método"] --> C
    H["Hash de carga"] --> C
    D["Predecesor"] --> C
    S["Sal"] --> C
    E["Época"] --> C
    C --> SHA["SHA-256"] --> ID["Operation ID"]
```

## Ciclo de vida

```mermaid
stateDiagram-v2
    [*] --> Propuesta
    Propuesta --> EnCola: proponente activo
    EnCola --> EnCola: aprobación única
    EnCola --> Ejecutada: quórum y espera
    EnCola --> Cancelada: guardián
    EnCola --> Caducada: fin de gracia
    Ejecutada --> [*]
    Cancelada --> [*]
    Caducada --> [*]
```

El proponente aporta la primera aprobación. Una identidad no aprueba dos veces. La ejecución exige época válida y predecesor ejecutado.

## Clases

| Cambio                    |   Quórum |    Espera | Revisión    |
| ------------------------- | -------: | --------: | ----------- |
| Operativo reversible      |      2/3 | 24 épocas | Operaciones |
| Parámetro económico       |      3/5 | 72 épocas | Riesgo      |
| Identidad o destino       |      4/5 | 96 épocas | Seguridad   |
| Restricción de emergencia | Guardián | Inmediata | Posterior   |

El guardián cancela o restringe; no amplía límites ni mueve reservas.

## Expediente

Toda propuesta contiene valor actual, valor nuevo, unidad, rango, simulación, impacto por vault, hash canónico, predecesores, observación y reversión. Cambios de binario, identidad, destino y parámetro económico se separan.
