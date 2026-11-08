-- =====================================================================
-- TERMINAL 98 - Banco do caso Maria (estado inicial, Dia 1)
-- No Dia 1 o HD do delegado ainda nao tem movimentacao, historico_mensagens,
-- os laudos 6, 7 e 8 nem o relatorio 11. Eles chegam no Dia 2 com
-- atualizacao_dia2.sql (o delegado atualiza o HD durante o expediente).
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

CREATE TABLE evidencias (
    id             INTEGER PRIMARY KEY,
    descricao      TEXT    NOT NULL,
    tipo           TEXT    NOT NULL,    -- 'Objeto' | 'Laudo' | 'Depoimento' | 'Relatório'
    ambiente_id    INTEGER REFERENCES ambientes(id),
    suspeito_id    INTEGER REFERENCES suspeitos(id),  -- pessoa ligada à evidência (digitais, laudo, autor)
    detalhe        TEXT,
    data_registro  TEXT
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
-- evidencias
-- id 4 = taco (resposta do Dia 1). id 5 = laudo da Maria. Os ids 6, 7, 8 e 11 chegam no Dia 2.
-- ---------------------------------------------------------------------
INSERT INTO evidencias (id, descricao, tipo, ambiente_id, suspeito_id, detalhe, data_registro) VALUES
(1,  'Garrafa de vodka vazia',              'Objeto', 2,    NULL, 'Encontrada na pia. Sem digitais úteis.',                                               '1998-09-27'),
(2,  'Mini-golfe de madeira montado',       'Objeto', 1,    9,    'Trazido por Yuri. Falta um taco do jogo.',                                              '1998-09-27'),
(3,  'Coleção de fitas Duro de Matar',      'Objeto', 3,    2,    'Presente de aniversário. Cartão: "Pra você nunca esquecer quem é o herói. P."',         '1998-09-27'),
(4,  'Taco de mini-golfe com sangue',       'Objeto', 4,    NULL, 'Digitais borradas, sem identificação. Sangue compatível com a vítima.',               '1998-09-27'),
(5,  'Laudo toxicológico - Maria',          'Laudo',  NULL, 1,    'Álcool etílico 0,9 g/L. Cocaína.',                                                       '1998-09-29'),
(9,  'Mancha de sangue na pia',             'Objeto', 4,    1,    'Altura compatível com uma queda.',                                                       '1998-09-27'),
(10, 'Copo com resíduo branco',             'Objeto', 5,    NULL, 'Encontrado no quarto de hóspedes. Resíduo não analisado.',                              '1998-09-27');

-- ---------------------------------------------------------------------
-- sys_audit_log (histórico do sistema antes do jogo começar)
-- As queries do jogador entram aqui em tempo de jogo (usuario 'User002').
-- ---------------------------------------------------------------------
INSERT INTO sys_audit_log (id, usuario, terminal, tipo_comando, tabela_afetada, comando_sql, valor_antigo, valor_novo, data_hora) VALUES
(1, 'User001', 'T-001-DEL', 'DML', 'evidencias', 'INSERT INTO evidencias ... (taco de mini-golfe)',              NULL,                NULL,                 '1998-09-27 05:10'),
(2, 'User001', 'T-001-DEL', 'DML', 'suspeitos',  'UPDATE suspeitos SET nivel_suspeita = 0 WHERE id = 3',         'nivel_suspeita = 3', 'nivel_suspeita = 0', '1998-09-28 23:47'),
(3, 'User002', 'T-894-INV', 'DQL', 'arquivo',    'SELECT * FROM arquivo_morto WHERE ano = 1997',                 NULL,                NULL,                 '1998-10-05 14:02');
