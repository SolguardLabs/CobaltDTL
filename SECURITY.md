# Seguridad De CobaltDTL

Este documento resume el modelo de seguridad operativo del simulador
CobaltDTL. El objetivo del sistema es mantener accounting determinista entre
reservas reales, creditos internos, tickets de redencion y liquidaciones por
batch.

## Modelo De Seguridad

- Cada vault esta vinculado a un unico activo de reserva.
- Los depositos y minting interno consumen saldo de cuenta antes de emitir
  creditos.
- Las redenciones se representan mediante tickets con epoch de desbloqueo.
- Las liquidaciones queman creditos del participante y aportan reserva al
  vault.
- Las politicas por vault controlan caps de deposito, tamano maximo de ticket,
  delays, batch limit, fees y disponibilidad de operaciones.

## Invariantes Esperadas

- La suma de creditos de cuenta mas creditos en cola debe coincidir con los
  creditos emitidos del vault.
- Ninguna cuenta puede gastar reservas o creditos que no posee.
- Los tickets liquidados no vuelven a ejecutarse.
- Los fees acumulados se separan del payout de usuario y se barren mediante
  una operacion explicita.
- La salida JSON debe ser determinista para el mismo fixture de entrada.

## Validaciones Automatizadas

La suite publica cubre:

- contrato de CLI y validacion de fixtures;
- deposito, minting, transferencia y sweep de fees;
- colas diferidas y batches parciales;
- cambios de indice y liquidaciones;
- reconciliacion de creditos emitidos frente a creditos asignados.

## Dependencias

El core C++ no utiliza dependencias externas. Node se usa para scripts,
compilacion y tests TypeScript. Dependabot cubre GitHub Actions y el ecosistema
npm.

## Alcance De Revision

La revision debe incluir:

- `src/ledger.*` para transiciones de estado;
- `src/pricing.*` para calculo de precio y supply;
- `src/risk.*` para admision de operaciones;
- fixtures de `tests/fixtures/` para expectativas de integracion.

Los reportes internos deben incluir escenario, fixture minimo, salida JSON
relevante, impacto economico y recomendacion de test de regresion.

