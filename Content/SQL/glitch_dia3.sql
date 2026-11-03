-- =====================================================================
-- TERMINAL 98 - Glitch do Dia 3 (Gli01)
-- Rodar com USQLiteBridge::ExecutarScriptSQL("glitch_dia3.sql") ao fechar o Diário do Dia 3.
-- André, logado como User002 a partir do terminal PREF-TI-07, coloca José na cena.
-- =====================================================================

UPDATE movimentacao SET ambiente_id = 3 WHERE id = 14;

UPDATE evidencias
SET suspeito_id = 3,
    detalhe = 'Digitais de José identificadas no cabo. Sangue compatível com a vítima.'
WHERE id = 4;

INSERT INTO sys_audit_log (usuario, terminal, tipo_comando, tabela_afetada, comando_sql, valor_antigo, valor_novo, data_hora) VALUES
('User002', 'PREF-TI-07', 'DML', 'movimentacao', 'UPDATE movimentacao SET ambiente_id = 3 WHERE id = 14', 'ambiente_id = 1', 'ambiente_id = 3', '1998-10-10 06:12'),
('User002', 'PREF-TI-07', 'DML', 'evidencias',   'UPDATE evidencias SET suspeito_id = 3, detalhe = ... WHERE id = 4', 'suspeito_id = NULL; detalhe = ''Digitais borradas, sem identificação. Sangue compatível com a vítima.''', 'suspeito_id = 3; detalhe = ''Digitais de José identificadas no cabo. Sangue compatível com a vítima.''', '1998-10-10 06:13');
