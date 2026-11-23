import json
import os
import urllib.error
import urllib.request

BASE = os.environ.get("ORPHANAGE_TEST_URL", "http://localhost:8080")


def call(path, method="GET", body=None, token=None):
    data = None if body is None else json.dumps(body).encode("utf-8")
    headers = {"Content-Type": "application/json"}
    if token:
        headers["Authorization"] = "Bearer " + token
    request = urllib.request.Request(
        BASE + path, data=data, headers=headers, method=method
    )
    try:
        with urllib.request.urlopen(request) as response:
            return response.status, json.loads(response.read())
    except urllib.error.HTTPError as error:
        return error.code, json.loads(error.read())


checks = []

status, health = call("/api/health")
checks.append(("health", status == 200 and health["status"] == "ok"))

status, login = call(
    "/api/auth/login",
    "POST",
    {"username": "admin", "password": "demo1234"},
)
admin_token = login.get("token")
checks.append(("administrator login", status == 200 and admin_token is not None))

status, dashboard = call("/api/dashboard", token=admin_token)
checks.append(("dashboard", status == 200 and "activeRecords" in dashboard))

status, created = call(
    "/api/records",
    "POST",
    {
        "firstName": "Smoke",
        "lastName": "Test",
        "dateOfBirth": "2019-02-10",
        "gender": "Other",
        "notes": "Temporary automated smoke record.",
    },
    admin_token,
)
record_id = created.get("id")
checks.append(("record create", status == 201 and record_id is not None))

status, search = call(
    "/api/records?q=Smoke&status=Active",
    token=admin_token,
)
checks.append(("record search", status == 200 and len(search["items"]) >= 1))

status, archive = call(
    "/api/records/" + str(record_id) + "/archive",
    "POST",
    token=admin_token,
)
checks.append(("record archive", status == 200 and archive.get("status") == "Archived"))

status, invalid = call(
    "/api/reports?from=2026-12-01&to=2026-01-01",
    token=admin_token,
)
checks.append(("invalid report date", status == 400))

status, staff = call(
    "/api/auth/login",
    "POST",
    {"username": "staff", "password": "demo1234"},
)
staff_token = staff.get("token")
status, forbidden = call("/api/users", token=staff_token)
checks.append(("staff permission", status == 403))

status, staff_delete = call(
    "/api/records/" + str(record_id),
    "DELETE",
    token=staff_token,
)
checks.append(("staff delete forbidden", status == 403))

status, admin_delete = call(
    "/api/records/" + str(record_id),
    "DELETE",
    token=admin_token,
)
checks.append(("administrator delete", status == 200))

failed = [name for name, passed in checks if not passed]
for name, passed in checks:
    print(("PASS" if passed else "FAIL") + ": " + name)

if failed:
    raise SystemExit("Failed checks: " + ", ".join(failed))
