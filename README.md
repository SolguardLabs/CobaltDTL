# CobaltDTL

![banner](./assets/banner.png)

CobaltDTL es un simulador C++ de vaults sinteticos con reservas reales,
creditos internos y redenciones diferidas por batches. El proyecto modela
depositos, minting interno, cambios de indice, liquidaciones y colas de
redencion para escenarios de auditoria reproducibles.

El binario no requiere servicios externos. Los tests TypeScript ejecutan la CLI
contra fixtures JSON y verifican el contrato de salida del ledger.

## Componentes

- `src/common.*`: cantidades, errores, identificadores y utilidades.
- `src/json.*`: parser y escritor JSON autocontenidos.
- `src/model.*`: entidades de dominio y carga de escenarios.
- `src/pricing.*`: snapshots de precio, supply y ratio de reservas.
- `src/risk.*`: politicas operativas de vault y limites por batch.
- `src/ledger.*`: ejecucion de operaciones y accounting del sistema.
- `src/engine.*`: carga, validacion y ejecucion de fixtures.
- `src/report.*`: reporte JSON estable para integraciones y tests.
- `src/main.cpp`: CLI `cobaltdtl`.

## Requisitos

- Node.js 24 o superior.
- Un compilador C++17 disponible como `c++`, `g++`, `clang++` o `cl`.

En Windows, el script de build tambien busca `vcvars64.bat` de Visual Studio.

## Uso

Compilar:

```bash
node scripts/build.mjs
```

Validar un escenario:

```bash
out/cobaltdtl validate tests/fixtures/base_cycle.json
```

Ejecutar un escenario con salida JSON:

```bash
out/cobaltdtl run tests/fixtures/base_cycle.json --json --events
```

## Tests

```bash
npm test
```

El script compila el binario y ejecuta:

```bash
node --test --experimental-strip-types "tests/node/*.test.ts"
```

## Escenarios

Los fixtures JSON definen:

- activos y precision contable;
- cuentas con reservas iniciales;
- vaults con indice, reservas, creditos emitidos y politicas;
- operaciones de deposito, minting, transferencia de creditos, redencion,
  avance de epoch, liquidacion, rebalanceo y sweep de fees;
- limites de batch, delays de redencion y caps por ticket.

Los importes son enteros. El indice usa escala fija `1_000_000`, donde
`1_000_000` representa precio base 1:1.

## CI

La validacion continua instala Node, toolchain C++ y ejecuta:

```bash
bash scripts/ci.sh
```

## Estado Del Lab

El repositorio esta disenado como un sistema de auditoria autocontenido. La
salida JSON final es el contrato principal para herramientas externas, tests de
regresion y analisis de escenarios.

