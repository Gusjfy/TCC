-- =====================================================================
-- TERMINAL 98 - Atualizacao do HD no Dia 2 (sexta, 09/10/1998)
-- Rodar com USQLiteBridge::ExecutarScriptSQL("atualizacao_dia2.sql") no
-- passo Script do comeco do Dia 2 (e no modo teste, quando comecar no Dia 2+).
-- O delegado e o escrivao atualizaram o HD durante o expediente: chegam
-- movimentacao, historico_mensagens, os laudos 6, 7 e 8 e o relatorio 11.
-- Pode rodar mais de uma vez sem duplicar nada (IF NOT EXISTS / OR IGNORE).
-- =====================================================================

CREATE TABLE IF NOT EXISTS movimentacao (
    id            INTEGER PRIMARY KEY,
    suspeito_id   INTEGER NOT NULL REFERENCES suspeitos(id),
    ambiente_id   INTEGER NOT NULL REFERENCES ambientes(id),
    hora_entrada  TEXT    NOT NULL,
    hora_saida    TEXT,
    fonte         TEXT    NOT NULL      -- de onde veio a informação
);

CREATE TABLE IF NOT EXISTS historico_mensagens (
    id            INTEGER PRIMARY KEY,
    remetente     TEXT    NOT NULL,
    destinatario  TEXT    NOT NULL,
    meio          TEXT    NOT NULL,     -- 'Pager' | 'Telefone'
    data_hora     TEXT    NOT NULL,
    conteudo      TEXT    NOT NULL
);

-- id 14 (José na sala de estar) é o registro que o glitch do Dia 3 altera.
-- Pedro não tem registro entre 02h20 e 03h05: ele não lembra (estava na cozinha com Julia).
INSERT OR IGNORE INTO movimentacao (id, suspeito_id, ambiente_id, hora_entrada, hora_saida, fonte) VALUES
(1,  2,  1, '1998-09-26 21:40', '1998-09-26 23:00', 'Depoimento'),
(2,  10, 7, '1998-09-26 22:20', '1998-09-27 00:10', 'Testemunha (Marta)'),
(3,  9,  1, '1998-09-26 22:30', '1998-09-27 00:40', 'Testemunha (Ana)'),
(4,  3,  1, '1998-09-26 23:20', '1998-09-27 00:40', 'Testemunha (Ana)'),
(5,  10, 1, '1998-09-27 00:10', '1998-09-27 00:40', 'Testemunha (Ana)'),
(6,  3,  3, '1998-09-27 00:40', '1998-09-27 01:00', 'Depoimento'),
(7,  1,  3, '1998-09-27 00:40', '1998-09-27 01:00', 'Depoimento (José)'),
(8,  2,  5, '1998-09-27 01:30', '1998-09-27 02:20', 'Depoimento (Yuri)'),
(9,  8,  5, '1998-09-27 01:30', '1998-09-27 03:25', 'Depoimento'),
(10, 9,  5, '1998-09-27 01:30', '1998-09-27 03:25', 'Depoimento'),
(11, 1,  3, '1998-09-27 02:20', NULL,               'Perícia'),
(12, 3,  3, '1998-09-27 02:35', '1998-09-27 02:45', 'Depoimento'),
(13, 10, 5, '1998-09-27 02:40', '1998-09-27 03:25', 'Depoimento (Yuri e João)'),
(14, 3,  1, '1998-09-27 02:45', '1998-09-27 03:20', 'Testemunha (Marta)'),
(15, 4,  8, '1998-09-27 02:40', '1998-09-27 02:50', 'Depoimento'),
(16, 4,  2, '1998-09-27 02:50', '1998-09-27 03:05', 'Depoimento'),
(17, 2,  6, '1998-09-27 03:05', '1998-09-27 03:12', 'Depoimento'),
(18, 2,  3, '1998-09-27 03:15', '1998-09-27 03:18', 'Perícia (digitais na porta)'),
(19, 5,  3, '1998-09-27 03:18', '1998-09-27 03:25', 'Depoimento'),
(20, 6,  7, '1998-09-26 21:50', '1998-09-27 03:25', 'Depoimento'),
(21, 7,  7, '1998-09-26 22:00', '1998-09-27 03:25', 'Depoimento');

-- id 7: o recado das 02h55 assinado "A." saiu do ramal do quarto de hóspedes.
INSERT OR IGNORE INTO historico_mensagens (id, remetente, destinatario, meio, data_hora, conteudo) VALUES
(1, 'Pedro',                        'Maria',              'Pager',    '1998-09-26 18:05', 'Chego cedo. Tenho uma surpresa. P.'),
(2, 'Julia',                        'Maria',              'Pager',    '1998-09-26 20:30', 'Posso ir? Queria te pedir desculpa por aquele dia.'),
(3, 'Yuri',                         'Maria',              'Telefone', '1998-09-26 21:10', 'Levo o mini-golfe. Pedido especial do teu amigo novo.'),
(4, 'Ana',                          'Marta',              'Pager',    '1998-09-26 23:50', 'Vem ver a Maria com o cara do bar kkkk'),
(5, 'José',                         'Maria',              'Pager',    '1998-09-27 00:55', 'Não acredito que você fez isso.'),
(6, 'José',                         'Maria',              'Pager',    '1998-09-27 02:46', 'Desculpa. A gente conversa amanhã.'),
(7, 'Ramal 3 (quarto de hóspedes)', 'Maria',              'Pager',    '1998-09-27 02:55', 'tô indo aí conversar. A.'),
(8, 'Carlos',                       'Emergência (193)',   'Telefone', '1998-09-27 03:19', 'Jovem desacordada no banheiro do quarto do casal. Tem sangue.');

INSERT OR IGNORE INTO evidencias (id, descricao, tipo, ambiente_id, suspeito_id, detalhe, data_registro) VALUES
(6,  'Laudo toxicológico - Pedro',          'Laudo',  NULL, 2,    'Álcool etílico 1,4 g/L. Cocaína. Sedativo (benzodiazepínico) em dose alta, origem desconhecida.', '1998-09-29'),
(7,  'Laudo toxicológico - João',           'Laudo',  NULL, 8,    'Álcool etílico 1,6 g/L. Cocaína. Portava 2 g da mesma substância.',                     '1998-09-29'),
(8,  'Laudo necroscópico - Maria',          'Laudo',  4,    1,    'Traumatismo craniano compatível com queda na pia. Petéquias nos olhos (não investigado).', '1998-09-30'),
(11, 'Caso 98-031 Maria: status EM ANDAMENTO', 'Relatório', NULL, NULL, 'Versão à imprensa: acidente por overdose. Investigação interna continua. Uso restrito.', '1998-10-08');

-- O log registra quem trouxe cada coisa. Sem id fixo: as queries do jogador tambem entram no log.
INSERT INTO sys_audit_log (usuario, terminal, tipo_comando, tabela_afetada, comando_sql, valor_antigo, valor_novo, data_hora)
SELECT 'User003', 'T-002-ESC', 'DML', 'movimentacao', 'CREATE TABLE movimentacao ... (digitalização dos depoimentos e da perícia)', NULL, '21 registros', '1998-10-09 09:40'
WHERE NOT EXISTS (SELECT 1 FROM sys_audit_log WHERE data_hora = '1998-10-09 09:40' AND usuario = 'User003');

INSERT INTO sys_audit_log (usuario, terminal, tipo_comando, tabela_afetada, comando_sql, valor_antigo, valor_novo, data_hora)
SELECT 'User003', 'T-002-ESC', 'DML', 'historico_mensagens', 'CREATE TABLE historico_mensagens ... (recados de pager e telefone da noite)', NULL, '8 registros', '1998-10-09 10:05'
WHERE NOT EXISTS (SELECT 1 FROM sys_audit_log WHERE data_hora = '1998-10-09 10:05' AND usuario = 'User003');

INSERT INTO sys_audit_log (usuario, terminal, tipo_comando, tabela_afetada, comando_sql, valor_antigo, valor_novo, data_hora)
SELECT 'User001', 'T-001-DEL', 'DML', 'evidencias', 'INSERT INTO evidencias ... (laudos toxicológicos de Pedro e João, laudo necroscópico)', NULL, 'ids 6, 7 e 8', '1998-10-09 11:20'
WHERE NOT EXISTS (SELECT 1 FROM sys_audit_log WHERE data_hora = '1998-10-09 11:20' AND usuario = 'User001');

INSERT INTO sys_audit_log (usuario, terminal, tipo_comando, tabela_afetada, comando_sql, valor_antigo, valor_novo, data_hora)
SELECT 'User001', 'T-001-DEL', 'DML', 'evidencias', 'UPDATE evidencias SET detalhe = ... WHERE id = 8', 'Causa: em análise', 'Traumatismo craniano compatível com queda na pia. Petéquias nos olhos (não investigado).', '1998-10-09 11:26'
WHERE NOT EXISTS (SELECT 1 FROM sys_audit_log WHERE data_hora = '1998-10-09 11:26' AND usuario = 'User001');

INSERT INTO sys_audit_log (usuario, terminal, tipo_comando, tabela_afetada, comando_sql, valor_antigo, valor_novo, data_hora)
SELECT 'User001', 'T-001-DEL', 'DML', 'evidencias', 'INSERT INTO evidencias ... (relatório caso 98-031)', NULL, 'status = EM ANDAMENTO', '1998-10-09 16:48'
WHERE NOT EXISTS (SELECT 1 FROM sys_audit_log WHERE data_hora = '1998-10-09 16:48' AND usuario = 'User001');

