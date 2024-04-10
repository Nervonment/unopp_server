CREATE TABLE user (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_name TEXT NOT NULL,
    password TEXT NOT NULL,
    sessdata INTEGER,
    slogan TEXT
);

CREATE TABLE friend_request (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    requester_id INTEGER,
    requestee_id INTEGER,
    UNIQUE (requester_id, requestee_id)
);

CREATE TABLE relation (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER,
    friend_id INTEGER, 
    unread INTEGER DEFAULT 0,
    UNIQUE (user_id, friend_id)
);