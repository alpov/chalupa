CREATE TABLE stats (
    id integer primary key autoincrement,
    dt numeric not null default (strftime('%s', 'now')),
    ip text,
    cnt integer,
    cnt_pwron integer,
    cnt_lowbatt integer,
    cnt_itplus integer,
    signalq integer,
    cnt_gsm integer
);

CREATE TABLE itp (
    id integer primary key autoincrement,
    dt numeric not null,
    itp integer,
    temp numeric,
    hygro numeric,
    lowbatt numeric
);

CREATE TABLE adc (
    id integer primary key autoincrement,
    dt numeric not null,
    idx integer,
    cnt_rx integer,
    tint numeric,
    text numeric,
    vbatt numeric,
    vpv numeric
);

CREATE VIEW itp31 as select * from itp where itp=31
/* itp31(id,dt,itp,"temp",hygro,lowbatt) */;

CREATE VIEW itp46 as select * from itp where itp=46
/* itp46(id,dt,itp,"temp",hygro,lowbatt) */;

CREATE VIEW itp62 as select * from itp where itp=62
/* itp62(id,dt,itp,"temp",hygro,lowbatt) */;

CREATE VIEW itp57 as select * from itp where itp=57
/* itp57(id,dt,itp,"temp",hygro,lowbatt) */;
