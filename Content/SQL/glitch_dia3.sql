-- =====================================================================
-- TERMINAL 98 - Glitch do Dia 3 (passo 040 do Dia 3, acao Script)
-- André, logado como User002 a partir do terminal PREF-TI-07, coloca José na cena.
-- Pode rodar de novo (replay, modo teste, Restaurar backup) sem duplicar:
-- so muda os dados se o log ainda nao tem as linhas do PREF-TI-07.
-- Assim, se o jogador ja desfez a linha 14, o replay nao refaz o glitch.
-- =====================================================================

UPDATE movimentacao SET ambiente_id = 3
WHERE id = 14 AND NOT EXISTS (SELECT 1 FROM sys_audit_log WHERE terminal = 'PREF-TI-07');

UPDATE evidencias
SET suspeito_id = 3,
    detalhe = 'Digitais de José identificadas no cabo. Sangue compatível com a vítima.'
WHERE id = 4 AND NOT EXISTS (SELECT 1 FROM sys_audit_log WHERE terminal = 'PREF-TI-07');

INSERT INTO sys_audit_log (usuario, terminal, tipo_comando, tabela_afetada, comando_sql, valor_antigo, valor_novo, data_hora)
SELECT 'User002', 'PREF-TI-07', 'DML', 'movimentacao', 'UPDATE movimentacao SET ambiente_id = 3 WHERE id = 14', 'ambiente_id = 1', 'ambiente_id = 3', '1998-10-10 06:12'
WHERE NOT EXISTS (SELECT 1 FROM sys_audit_log WHERE terminal = 'PREF-TI-07');

INSERT INTO sys_audit_log (usuario, terminal, tipo_comando, tabela_afetada, comando_sql, valor_antigo, valor_novo, data_hora)
SELECT 'User002', 'PREF-TI-07', 'DML', 'evidencias', 'UPDATE evidencias SET suspeito_id = 3, detalhe = ... WHERE id = 4', 'suspeito_id = NULL; detalhe = ''Digitais borradas, sem identificação. Sangue compatível com a vítima.''', 'suspeito_id = 3; detalhe = ''Digitais de José identificadas no cabo. Sangue compatível com a vítima.''', '1998-10-10 06:13'
WHERE NOT EXISTS (SELECT 1 FROM sys_audit_log WHERE terminal = 'PREF-TI-07' AND tabela_afetada = 'evidencias');
