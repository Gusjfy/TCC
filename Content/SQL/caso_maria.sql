-- =====================================================================
-- TERMINAL 98 - Banco do caso Maria (estado inicial, Dia 1)
-- Recriado do zero por USQLiteBridge::RecriarBanco.
-- Datas no formato 'AAAA-MM-DD HH:MM' para ORDER BY e comparacoes funcionarem.
-- Festa: sab 26/09/1998. Crime: dom 27/09/1998, 03h00.
--
-- O DOSSIE DO JOGADOR NAO E CRIADO AQUI. Pedro cria a propria tabela com
-- CREATE TABLE no fim do Dia 2 (Con04). O RecriarBanco copia essa tabela
-- para o banco novo. Gabaritos do CREATE/INSERT/ALTER: guias/banco_e_gabaritos.md.
-- =====================================================================

CREATE TABLE suspeitos (
    id              INTEGER PRIMARY KEY,
    nome            TEXT    NOT NULL,
    idade           INTEGER,
    ocupacao        TEXT,
    relacao_vitima  TEXT,
    status          TEXT    NOT NULL,   -- 'Vítima' | 'Testemunha'
    nivel_suspeita  INTEGER,            -- 0 a 5, definido pela polícia (NULL para a vítima)
    depoimento      TEXT
);

CREATE TABLE ambientes (
    id         INTEGER PRIMARY KEY,
    nome       TEXT    NOT NULL,
    andar      INTEGER NOT NULL,
    descricao  TEXT
);

CREATE TABLE movimentacao (
    id            INTEGER PRIMARY KEY,
    suspeito_id   INTEGER NOT NULL REFERENCES suspeitos(id),
    ambiente_id   INTEGER NOT NULL REFERENCES ambientes(id),
    hora_entrada  TEXT    NOT NULL,
    hora_saida    TEXT,
    fonte         TEXT    NOT NULL      -- de onde veio a informação
);

CREATE TABLE evidencias (
    id             INTEGER PRIMARY KEY,
    descricao      TEXT    NOT NULL,
    tipo           TEXT    NOT NULL,    -- 'Objeto' | 'Laudo' | 'Depoimento' | 'Relatório'
    ambiente_id    INTEGER REFERENCES ambientes(id),
    suspeito_id    INTEGER REFERENCES suspeitos(id),  -- pessoa ligada à evidência (digitais, laudo, autor)
    detalhe        TEXT,
    data_registro  TEXT
);

CREATE TABLE historico_mensagens (
    id            INTEGER PRIMARY KEY,
    remetente     TEXT    NOT NULL,
    destinatario  TEXT    NOT NULL,
    meio          TEXT    NOT NULL,     -- 'Pager' | 'Telefone'
    data_hora     TEXT    NOT NULL,
    conteudo      TEXT    NOT NULL
);

CREATE TABLE sys_audit_log (
    id              INTEGER PRIMARY KEY,
    usuario         TEXT    NOT NULL,
    terminal        TEXT    NOT NULL DEFAULT 'T-894-INV',
    tipo_comando    TEXT    NOT NULL,   -- 'DQL' | 'DML'
    tabela_afetada  TEXT,
    comando_sql     TEXT    NOT NULL,
    valor_antigo    TEXT,
    valor_novo      TEXT,
    data_hora       TEXT    NOT NULL
);

-- ---------------------------------------------------------------------
-- suspeitos (ids fixos: as falas e os gabaritos dependem deles)
-- ---------------------------------------------------------------------
INSERT INTO suspeitos (id, nome, idade, ocupacao, relacao_vitima, status, nivel_suspeita, depoimento) VALUES
(1,  'Maria',  18, 'Estudante',                    'Vítima',                     'Vítima',     NULL, NULL),
(2,  'Pedro',  19, 'Estagiário de informática',    'Amigo de infância',          'Testemunha', 3,    'Não lembro de muita coisa. Fui pedir desculpas e encontrei a Maria no banheiro.'),
(3,  'José',   21, 'Guitarrista',                  'Amigo',                      'Testemunha', 0,    'Discuti com ela e desci pra sala. Fiquei lá até a polícia chegar.'),
(4,  'Julia',  18, 'Estudante de medicina',        'Vizinha',                    'Testemunha', 1,    'Estava na cozinha. Não vi nada de estranho.'),
(5,  'Carlos', 20, 'Atendente de sorveteria',      'Amigo',                      'Testemunha', 1,    'Subi pra chamar a Maria e encontrei o Pedro em choque no quarto. Liguei pra emergência.'),
(6,  'Marta',  20, 'Resenhista de livros',         'Amiga',                      'Testemunha', 0,    'Fiquei no quintal com a Ana quase a noite toda. Conversei bastante com o André, um rapaz muito educado.'),
(7,  'Ana',    20, 'Estudante',                    'Amiga',                      'Testemunha', 0,    'Fiquei na churrasqueira. Mais cedo ouvi a Maria rindo com um cara no corredor.'),
(8,  'João',   30, 'Baterista',                    'Colega de banda',            'Testemunha', 2,    'Apaguei no quarto de hóspedes. Não lembro de nada.'),
(9,  'Yuri',   24, 'Empresário da banda',          'Ex-namorado',                'Testemunha', 1,    'Fiquei no quarto de hóspedes com o João e o André. Ninguém saiu de lá.'),
(10, 'André',  26, 'Técnico de TI da prefeitura',  'Conhecido recente',          'Testemunha', 0,    'Passei a madrugada no quarto de hóspedes com o Yuri e o João.');

-- ---------------------------------------------------------------------
-- ambientes (casa dos pais de Maria)
-- id 2 vem com o nome digitado errado ('Cozinah'). O Anonimo usa isso no Dia 3
-- para ensinar UPDATE num registro inofensivo antes da restauracao de José.
-- ---------------------------------------------------------------------
INSERT INTO ambientes (id, nome, andar, descricao) VALUES
(1, 'Sala de estar',          1, 'Onde montaram o mini-golfe.'),
(2, 'Cozinah',                1, 'Bebidas e a porta para o quintal.'),  -- erro de digitacao proposital: treino de UPDATE do Dia 3 (iniciante)
(3, 'Suíte (quarto dos pais)',2, 'Quarto do casal, com banheiro próprio.'),
(4, 'Banheiro da suíte',      2, 'Local onde o corpo foi encontrado.'),
(5, 'Quarto de hóspedes',     2, 'Onde parte do grupo usou drogas.'),
(6, 'Banheiro do corredor',   2, 'Banheiro social do andar de cima.'),
(7, 'Quintal (churrasqueira)',1, 'Área externa com a churrasqueira.'),
(8, 'Corredor',               2, 'Liga os quartos e os banheiros do andar de cima.');

-- ---------------------------------------------------------------------
-- movimentacao
-- id 14 (José na sala de estar) é o registro que o glitch do Dia 3 altera.
-- Pedro não tem registro entre 02h20 e 03h05: ele não lembra (estava na cozinha com Julia).
-- ---------------------------------------------------------------------
INSERT INTO movimentacao (id, suspeito_id, ambiente_id, hora_entrada, hora_saida, fonte) VALUES
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

-- ---------------------------------------------------------------------
-- evidencias
-- id 4 = taco (resposta do Dia 1). ids 5 a 8 = laudos (Dia 2). id 11 = caso ainda aberto.
-- ---------------------------------------------------------------------
INSERT INTO evidencias (id, descricao, tipo, ambiente_id, suspeito_id, detalhe, data_registro) VALUES
(1,  'Garrafa de vodka vazia',              'Objeto', 2,    NULL, 'Encontrada na pia. Sem digitais úteis.',                                               '1998-09-27'),
(2,  'Mini-golfe de madeira montado',       'Objeto', 1,    9,    'Trazido por Yuri. Falta um taco do jogo.',                                              '1998-09-27'),
(3,  'Coleção de fitas Duro de Matar',      'Objeto', 3,    2,    'Presente de aniversário. Cartão: "Pra você nunca esquecer quem é o herói. P."',         '1998-09-27'),
(4,  'Taco de mini-golfe com sangue',       'Objeto', 4,    NULL, 'Digitais borradas, sem identificação. Sangue compatível com a vítima.',               '1998-09-27'),
(5,  'Laudo toxicológico - Maria',          'Laudo',  NULL, 1,    'Álcool etílico 0,9 g/L. Cocaína.',                                                       '1998-09-29'),
(6,  'Laudo toxicológico - Pedro',          'Laudo',  NULL, 2,    'Álcool etílico 1,4 g/L. Cocaína. Sedativo (benzodiazepínico) em dose alta, origem desconhecida.', '1998-09-29'),
(7,  'Laudo toxicológico - João',           'Laudo',  NULL, 8,    'Álcool etílico 1,6 g/L. Cocaína. Portava 2 g da mesma substância.',                     '1998-09-29'),
(8,  'Laudo necroscópico - Maria',          'Laudo',  4,    1,    'Traumatismo craniano compatível com queda na pia. Petéquias nos olhos (não investigado).', '1998-09-30'),
(9,  'Mancha de sangue na pia',             'Objeto', 4,    1,    'Altura compatível com uma queda.',                                                       '1998-09-27'),
(10, 'Copo com resíduo branco',             'Objeto', 5,    NULL, 'Encontrado no quarto de hóspedes. Resíduo não analisado.',                              '1998-09-27'),
(11, 'Caso 98-031 Maria: status EM ANDAMENTO', 'Relatório', NULL, NULL, 'Versão à imprensa: acidente por overdose. Investigação interna continua. Uso restrito.', '1998-10-08');

-- ---------------------------------------------------------------------
-- historico_mensagens (pager e telefone da casa)
-- id 7: o recado das 02h55 assinado "A." saiu do ramal do quarto de hóspedes.
-- ---------------------------------------------------------------------
INSERT INTO historico_mensagens (id, remetente, destinatario, meio, data_hora, conteudo) VALUES
(1, 'Pedro',                        'Maria',              'Pager',    '1998-09-26 18:05', 'Chego cedo. Tenho uma surpresa. P.'),
(2, 'Julia',                        'Maria',              'Pager',    '1998-09-26 20:30', 'Posso ir? Queria te pedir desculpa por aquele dia.'),
(3, 'Yuri',                         'Maria',              'Telefone', '1998-09-26 21:10', 'Levo o mini-golfe. Pedido especial do teu amigo novo.'),
(4, 'Ana',                          'Marta',              'Pager',    '1998-09-26 23:50', 'Vem ver a Maria com o cara do bar kkkk'),
(5, 'José',                         'Maria',              'Pager',    '1998-09-27 00:55', 'Não acredito que você fez isso.'),
(6, 'José',                         'Maria',              'Pager',    '1998-09-27 02:46', 'Desculpa. A gente conversa amanhã.'),
(7, 'Ramal 3 (quarto de hóspedes)', 'Maria',              'Pager',    '1998-09-27 02:55', 'tô indo aí conversar. A.'),
(8, 'Carlos',                       'Emergência (193)',   'Telefone', '1998-09-27 03:19', 'Jovem desacordada no banheiro do quarto do casal. Tem sangue.');

-- ---------------------------------------------------------------------
-- sys_audit_log (histórico do sistema antes do jogo começar)
-- As queries do jogador entram aqui em tempo de jogo (usuario 'User002').
-- ---------------------------------------------------------------------
INSERT INTO sys_audit_log (id, usuario, terminal, tipo_comando, tabela_afetada, comando_sql, valor_antigo, valor_novo, data_hora) VALUES
(1, 'User001', 'T-001-DEL', 'DML', 'evidencias', 'INSERT INTO evidencias ... (taco de mini-golfe)',              NULL,                NULL,                 '1998-09-27 05:10'),
(2, 'User001', 'T-001-DEL', 'DML', 'suspeitos',  'UPDATE suspeitos SET nivel_suspeita = 0 WHERE id = 3',         'nivel_suspeita = 3', 'nivel_suspeita = 0', '1998-09-28 23:47'),
(3, 'User001', 'T-001-DEL', 'DML', 'evidencias', 'UPDATE evidencias SET detalhe = ... WHERE id = 8',             'Causa: em análise', 'Traumatismo craniano compatível com queda na pia. Petéquias nos olhos (não investigado).', '1998-09-30 18:02'),
(4, 'User002', 'T-894-INV', 'DQL', 'arquivo',    'SELECT * FROM arquivo_morto WHERE ano = 1997',                 NULL,                NULL,                 '1998-10-05 14:02'),
(5, 'User001', 'T-001-DEL', 'DML', 'evidencias', 'INSERT INTO evidencias ... (relatório caso 98-031)',           NULL,                'status = EM ANDAMENTO', '1998-10-08 22:31');
