-- =====================================================================
-- TERMINAL 98 - Aditamento do depoimento da Marta (passo 005 do Dia 3, acao Script)
-- Na sexta a noite (Dia 2) a Marta liga para o Pedro, conta que cruzou com o
-- André no corredor de cima e vai ao plantão. O plantão digita o aditamento no
-- sábado, antes do Pedro chegar. O banco do Dia 3 já começa com ele.
-- Pode rodar de novo (replay, modo teste, Restaurar backup) sem duplicar.
-- =====================================================================

INSERT OR IGNORE INTO evidencias (id, descricao, tipo, ambiente_id, suspeito_id, detalhe, data_registro) VALUES
(12, 'Depoimento - Marta (aditamento)', 'Depoimento', 8, 10,
 'Por volta das 03h subi pro banheiro e cruzei com o André no corredor de cima, saindo do lado do quarto de hóspedes e indo pro lado da suíte. Ele segurou a porta do banheiro pra mim. Na ida e na volta, o José estava no sofá da sala.',
 '1998-10-09');

INSERT OR IGNORE INTO movimentacao (id, suspeito_id, ambiente_id, hora_entrada, hora_saida, fonte) VALUES
(22, 10, 8, '1998-09-27 02:58', '1998-09-27 03:03', 'Testemunha (Marta, aditamento)');

INSERT INTO sys_audit_log (usuario, terminal, tipo_comando, tabela_afetada, comando_sql, valor_antigo, valor_novo, data_hora)
SELECT 'User005', 'T-003-PLA', 'DML', 'evidencias', 'INSERT INTO evidencias ... (aditamento de depoimento: Marta, plantão de 09/10 23h10)', NULL, 'evidencias 12; movimentacao 22', '1998-10-10 13:50'
WHERE NOT EXISTS (SELECT 1 FROM sys_audit_log WHERE usuario = 'User005' AND data_hora = '1998-10-10 13:50');
