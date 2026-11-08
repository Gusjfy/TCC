-- Dia 2, passo 210 (GlitchAdmin): o Anônimo troca a permissão do Pedro.
-- A troca fica no log pela conta do suporte da prefeitura. Pista do Dia 3.
-- Idempotente: pode rodar de novo no replay/backup sem duplicar.
INSERT INTO sys_audit_log (usuario, terminal, tipo_comando, tabela_afetada, comando_sql, valor_antigo, valor_novo, data_hora)
SELECT 'User004', 'T-PRF-SUP', 'DCL', 'permissoes', 'GRANT ALL ON * TO User002', 'perfil = estagiario', 'perfil = admin', '1998-10-09 21:47'
WHERE NOT EXISTS (SELECT 1 FROM sys_audit_log WHERE usuario = 'User004' AND data_hora = '1998-10-09 21:47');
