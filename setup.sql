CREATE TABLE user (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_name TEXT NOT NULL,
    password TEXT NOT NULL,
    icon TEXT,
    sessdata INTEGER
);