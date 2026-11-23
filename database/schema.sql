PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    full_name TEXT NOT NULL,
    username TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    role TEXT NOT NULL CHECK(role IN ('Administrator', 'Staff')),
    active INTEGER NOT NULL DEFAULT 1 CHECK(active IN (0, 1)),
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS records (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    first_name TEXT NOT NULL,
    last_name TEXT NOT NULL,
    date_of_birth TEXT,
    gender TEXT CHECK(gender IN ('Female', 'Male', 'Other', 'Not specified')),
    status TEXT NOT NULL DEFAULT 'Active'
        CHECK(status IN ('Active', 'Archived')),
    notes TEXT,
    created_by INTEGER,
    updated_by INTEGER,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(created_by) REFERENCES users(id),
    FOREIGN KEY(updated_by) REFERENCES users(id)
);

CREATE INDEX IF NOT EXISTS idx_records_name
    ON records(last_name, first_name);
CREATE INDEX IF NOT EXISTS idx_records_status
    ON records(status);
CREATE INDEX IF NOT EXISTS idx_records_updated
    ON records(updated_at);

CREATE TABLE IF NOT EXISTS activity_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER,
    action TEXT NOT NULL,
    details TEXT,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(user_id) REFERENCES users(id)
);

INSERT INTO records(first_name, last_name, date_of_birth, gender, status, notes)
SELECT 'Asha', 'Kapoor', '2018-04-12', 'Female', 'Active',
       'Fictional demo record; school enrollment in progress.'
WHERE NOT EXISTS (SELECT 1 FROM records WHERE first_name='Asha' AND last_name='Kapoor');

INSERT INTO records(first_name, last_name, date_of_birth, gender, status, notes)
SELECT 'Rohan', 'Mehta', '2016-09-28', 'Male', 'Active',
       'Fictional demo record; regular health check completed.'
WHERE NOT EXISTS (SELECT 1 FROM records WHERE first_name='Rohan' AND last_name='Mehta');

INSERT INTO records(first_name, last_name, date_of_birth, gender, status, notes)
SELECT 'Mira', 'Shah', '2014-01-19', 'Female', 'Archived',
       'Fictional demo record; archived for a documented placement review.'
WHERE NOT EXISTS (SELECT 1 FROM records WHERE first_name='Mira' AND last_name='Shah');
