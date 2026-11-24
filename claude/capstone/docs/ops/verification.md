# Verificação de release — v1.4.2+k7

1. Baixe o artefato `fw-<sha>` do pipeline apontado no release.
2. Confira a string de versão na UART: `FW v1.4.2+k7`.
3. Rode `sha256sum firmware.bin` e compare com o valor anexado ao release.
4. Reexecute o HIL: veja `reports/hil/results.xml` e `reports/hil/summary.txt`.
5. Se falhar: `/rewind` no commit anterior, abra Issue com label `regression` e vincule o REQ afetado.

## Regras

- Nunca grave firmware com `k` diferente da revisão da placa.
- Nunca promova um binário cujo job `hil:gpio` não tenha rodado no commit exato da tag.
