# Seguridad operativa

## Separación de funciones

| Función      |   Solicita | Aprueba |        Ejecuta | Concilia | Cancela |
| ------------ | ---------: | ------: | -------------: | -------: | ------: |
| Operaciones  |         Sí |      No | Según política |       No |      No |
| Gobernador   |         Sí |      Sí |    Tras espera |       No |      No |
| Conciliación |         No |      No |             No |       Sí |      No |
| Guardián     |         No |      No |             No |  Lectura |      Sí |
| Plataforma   | Despliegue |      No |             No | Métricas |      No |

```mermaid
flowchart TD
    REQUEST["Comando autenticado"] --> SCHEMA{"Esquema"}
    SCHEMA -->|Incorrecto| REJECT["Rechazo"]
    SCHEMA -->|Correcto| REFS{"Referencias"}
    REFS -->|Ausentes| REJECT
    REFS -->|Válidas| POLICY{"Estado y límites"}
    POLICY -->|No| REJECT
    POLICY -->|Sí| VERSION{"Versión vigente"}
    VERSION -->|Conflicto| RETRY["Releer"]
    VERSION -->|Coincide| COMMIT["Asiento, ticket y evento"]
    COMMIT --> RECON["Conciliación"]
```

## Controles

- TLS y credenciales efímeras en el borde.
- Lista de métodos y destinos explícita.
- Cuerpo, timeout y cuota limitados.
- Idempotencia ligada al hash canónico del comando.
- Reloj de épocas monotónico.
- Escritura cerrada si no puede verificarse estado o versión.
- Diario y copias con acceso de solo lectura independiente.

## Cadena de suministro

```mermaid
flowchart LR
    SOURCE["Commit revisado"] --> MATRIX["CI Ubuntu y Windows"]
    LOCK["Dependencias fijadas"] --> MATRIX
    CPP["C++17 con warnings estrictos"] --> MATRIX
    MATRIX --> MAIN["main"]
    MAIN --> PROD["production"]
    MAIN --> TAG["Etiqueta anotada"]
    TAG --> RELEASE["Release"]
    PROD --> INTEGRITY["Integridad"]
    RELEASE --> INTEGRITY
```

Los workflows usan permisos de lectura. Un permiso adicional requiere revisión de propietario y alcance mínimo.

## Incidente

Preservar snapshot y eventos, cerrar operaciones afectadas, reconciliar por activo y vault, identificar la primera secuencia divergente y recuperar mediante asientos explícitos. Nunca se edita un evento histórico. La reapertura requiere dos ventanas conformes.

## Datos

Los identificadores son referencias técnicas. Datos de entidad o persona se mantienen fuera del núcleo, cifrados y con retención independiente. Los fixtures versionados contienen datos sintéticos.
