# Orphanage Record System

Academic NGO record-management prototype built in C++.

## Stack choices

- C++17
- `cpp-httplib` for a small local HTTP server
- SQLite 3 for persistent local storage
- OpenSSL PBKDF2-SHA256 for password hashing
- CMake for builds
- Responsive HTML, CSS, and JavaScript

These are implementation choices, not additional assignment requirements.

## Project status

Implemented: authentication, role-aware navigation, dashboard, record CRUD,
soft archiving, search and filters, date-range reports, and staff account creation.

## Prerequisites

- CMake 3.20+
- A C++17 compiler
- SQLite 3 development headers
- OpenSSL development headers
- Internet access on the first build for CMake to fetch `cpp-httplib` and `nlohmann/json`

macOS example:

```sh
brew install cmake sqlite openssl
```

## Setup and run

```sh
cmake -S . -B build
cmake --build build
./build/orphanage_system
```

Open `http://localhost:8080`.

Optional environment variables:

```sh
ORPHANAGE_PORT=8080
ORPHANAGE_DB=orphanage.db
ORPHANAGE_WEB_DIR=web
```

## Database

The first start initializes [database/schema.sql](database/schema.sql). The configured SQLite database is created automatically.

## Demo accounts

All demo accounts use the following password:

| Username | Role | Password |
|---|---|---|
| `admin` | Administrator | `demo1234` |
| `staff` | Staff | `demo1234` |

Demo records are fictional and exist only to demonstrate the interface.

## Record fields

The prototype collects the minimum fields needed for an academic record:

| Field | Purpose |
|---|---|
| First name, last name | Identify a record |
| Date of birth | Age context and record search/reporting |
| Gender | Optional demographic filter; "Not specified" is available |
| Notes | Brief operational context, limited to 2000 characters |

Sensitive or legal information is deliberately not collected. Real NGO use
requires qualified safeguarding, consent, retention, and access-policy review.

## API overview

- `POST /api/auth/login`
- `POST /api/auth/logout`
- `GET /api/auth/me`
- `GET /api/dashboard`
- `GET /api/records`, `POST /api/records`
- `GET /api/records/:id`, `PUT /api/records/:id`
- `POST /api/records/:id/archive`
- `DELETE /api/records/:id` (Administrator only)
- `GET /api/reports`
- `GET /api/users`, `POST /api/users`

Protected routes require `Authorization: Bearer <session-token>`.

## Requirements-to-code mapping

| Requirement | Evidence |
|---|---|
| Classes and objects | [src/domain.h](src/domain.h): `User`, `StaffUser`, `Administrator`, `Record`; [src/services.h](src/services.h): `AuthService`, `RecordService`, `Report`, `Manager` |
| Constructors | `User`, `StaffUser`, `Administrator`, `Record`, `Database`, and service constructors |
| Encapsulation | Private/protected domain state with accessors in [src/domain.h](src/domain.h); database and session state kept in service classes |
| Inheritance | `StaffUser` and `Administrator` derive from `User` in [src/domain.h](src/domain.h) |
| Function overloading | `RecordService::search(query)`, `RecordService::search(query,status,gender)`; `Report::generate()` and `Report::generate(from,to)` |
| Exception handling | `ValidationException`, HTTP route catch boundaries, and database/runtime errors in [src/main.cpp](src/main.cpp) |
| Password security | PBKDF2-SHA256 implementation in [src/password.cpp](src/password.cpp); plaintext passwords are never stored |
| Database persistence | [database/schema.sql](database/schema.sql), [src/database.cpp](src/database.cpp), SQLite prepared statements |
| Role permissions | [src/services.cpp](src/services.cpp) `hasPermission()` and administrator-only user routes |

## Manual demonstration walkthrough

1. Start the application and open `http://localhost:8080`.
2. Sign in with `admin` / `demo1234`.
3. Review the dashboard counts and recent records.
4. Open **Records**, add a fictional record, edit it, and archive it.
5. Open **Search**, filter by name and status, and confirm no-result behavior.
6. Open **Reports**, select a date range, and explain the four meaningful metrics.
7. Open **User Management**, create a fictional staff account.
8. Log out and log in as `staff` / `demo1234`; confirm User Management is hidden.
9. Try the staff account to verify it cannot access `/api/users`.

## Suggested test scenarios

| Area | Scenario | Success |
|---|---|---|
| Registration | Create a user with an 8+ character password | User is stored with a PBKDF2 hash and not a plaintext password |
| Login | Valid admin credentials | Session token and user role are returned |
| Login | Wrong password or unknown user | Helpful 401 response, no account detail exposed |
| Permissions | Staff calls `/api/users` | 403 response |
| Record create | Missing first or last name | 400 validation response |
| Record update | Change existing record | `updated_at` and `updated_by` change |
| Archive | Confirm archive action | Status becomes Archived; record is retained |
| Delete | Confirm permanent deletion | Administrator can remove the record; staff receives 403 |
| Search | Matching and unmatched names | Matching rows or clear no-results state |
| Search | Invalid status or gender | 400 validation response |
| Reports | Start date after end date | 400 validation response |
| Error handling | Stop SQLite/database access | Generic error response; server remains controlled |

## Automated tests

The C++ test executable verifies password hashing, record validation, and SQLite
schema initialization:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Run the separate API smoke checks while the server is running:

```sh
python3 tests/api_smoke.py
```

## Team ownership and integration

Proposed for three students:

1. **Backend/domain student:** domain classes, auth, routes, services, and validation.
2. **Database student:** schema, constraints, indexes, migrations, seed data, and SQL statements.
3. **Frontend student:** HTML/CSS/JavaScript, responsive states, dialogs, and API integration.

Shared boundary: HTTP JSON contracts in [src/main.cpp](src/main.cpp). Integrate in
this sequence: schema → database/repositories → login API → record/report APIs →
frontend pages → role and error-state review.

## Git workflow

- Use branches such as `feature/auth-api`, `feature/database`, or `feature/frontend`.
- Make small commits with imperative messages, for example `add prepared record search`.
- Open a pull request or peer review each feature branch before merging.
- Never commit `.db`, secrets, or `.env` files.

Do not create remote changes unless the team explicitly requests them.

## Project directory

```text
.
├── CMakeLists.txt
├── README.md
├── .env.example
├── .gitignore
├── database/
│   └── schema.sql
├── src/
│   ├── main.cpp
│   ├── domain.h
│   ├── database.h
│   ├── database.cpp
│   ├── password.h
│   ├── password.cpp
│   ├── services.h
│   └── services.cpp
├── tests/
│   ├── ors_tests.cpp
│   └── api_smoke.py
└── web/
    ├── index.html
    ├── styles.css
    └── app.js
```

## Concise report and presentation outline

1. Problem and NGO workflow.
2. Assignment requirements and stack choices.
3. System architecture and database relationships.
4. OOP evidence: each required concept with a code example.
5. Security and role permissions.
6. Demo of login, records, search, reports, and user management.
7. Limitations and suggested real-world safeguards.
8. Team contributions and reflection.

## Troubleshooting

- `SQLite3` or `OpenSSL` not found: install development packages and configure CMake again.
- Port already in use: run with `ORPHANAGE_PORT=8081`.
- Database schema seems stale: stop the server and remove `orphanage.db`, then restart.
- Static page does not open: run the server from the repository root so `web/` and `database/schema.sql` resolve correctly.

## Current limitations

This is a local academic prototype: sessions live in memory and reset on restart,
there is no browser CSRF protection or HTTPS, and real safeguarding and retention
rules must be added before actual NGO use.
