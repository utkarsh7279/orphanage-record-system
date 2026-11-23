const loginView = document.querySelector("#login-view");
const appView = document.querySelector("#app-view");
const pageError = document.querySelector("#page-error");
const recordDialog = document.querySelector("#record-dialog");
const userDialog = document.querySelector("#user-dialog");
const deleteDialog = document.querySelector("#delete-dialog");
let token = localStorage.getItem("ors_token");
let currentUser = null;
let pendingDeleteId = null;
const apiBase = (document.querySelector('meta[name="api-base"]')?.content || "").replace(/\/$/, "");

function escapeHtml(value) {
  return String(value ?? "").replace(/[&<>"']/g, (character) => ({
    "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#039;"
  })[character]);
}

async function api(path, options = {}) {
  const headers = {"Content-Type": "application/json", ...(options.headers || {})};
  if (token) headers.Authorization = `Bearer ${token}`;
  const response = await fetch(`${apiBase}${path}`, {...options, headers});
  let body = {};
  try { body = await response.json(); } catch {}
  if (!response.ok) throw new Error(body.error || "The request could not be completed.");
  return body;
}

function showPageError(message) {
  pageError.hidden = false;
  pageError.textContent = message;
}

function clearPageError() {
  pageError.hidden = true;
  pageError.textContent = "";
}

let toastTimer = null;
function showToast(message) {
  const toast = document.querySelector("#toast");
  toast.textContent = message;
  toast.hidden = false;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => {
    toast.hidden = true;
  }, 3200);
}

function showView(name) {
  document.querySelectorAll(".page-view").forEach((view) => {
    view.hidden = view.id !== `${name}-view`;
  });
  document.querySelectorAll(".nav-button").forEach((button) => {
    button.classList.toggle("active", button.dataset.view === name);
  });
  document.querySelector(".sidebar").classList.remove("open");
  document.querySelector("#menu-button").setAttribute("aria-expanded", "false");
  clearPageError();
  if (name === "dashboard") loadDashboard();
  if (name === "records") loadRecords();
  if (name === "search") loadSearch();
  if (name === "reports") loadReport();
  if (name === "users") loadUsers();
}

async function loadDashboard() {
  try {
    const dashboard = await api("/api/dashboard");
    document.querySelector("#active-count").textContent = dashboard.activeRecords;
    document.querySelector("#archived-count").textContent = dashboard.archivedRecords;
    const recent = document.querySelector("#recent-list");
    if (!dashboard.recentActivity.length) {
      recent.innerHTML = '<p class="muted">No recently updated records.</p>';
      return;
    }
    recent.innerHTML = dashboard.recentActivity.map((item) => `
      <div class="record-list-item">
        <strong>${escapeHtml(item.name)}</strong>
        <span>Record ${item.id} · ${escapeHtml(item.updatedAt)}</span>
      </div>`).join("");
  } catch (error) {
    showPageError(error.message);
  }
}

function recordTable(items, includeStatus = true) {
  if (!items.length) {
    return '<div class="empty-state">No records match the current filters.</div>';
  }
  return `
    <div class="table-wrap">
      <table>
        <thead><tr><th>ID</th><th>Name</th><th>Date of birth</th>
          ${includeStatus ? "<th>Status</th>" : ""}
          <th>Updated</th><th>Actions</th></tr></thead>
        <tbody>${items.map((record) => `
          <tr>
            <td>${record.id}</td>
            <td>${escapeHtml(record.firstName)} ${escapeHtml(record.lastName)}</td>
            <td>${escapeHtml(record.dateOfBirth || "Not provided")}</td>
            ${includeStatus ? `<td><span class="status-badge status-${record.status.toLowerCase()}">${escapeHtml(record.status)}</span></td>` : ""}
            <td>${escapeHtml(record.updatedAt)}</td>
            <td><div class="table-actions">
              <button class="button-small" data-edit-record="${record.id}">Edit</button>
              ${record.status === "Active"
                ? `<button class="button-small danger-button" data-archive-record="${record.id}">Archive</button>`
                : ""}
              ${currentUser?.role === "Administrator"
                ? `<button class="button-small delete-button" data-delete-record="${record.id}" data-record-name="${escapeHtml(record.firstName + " " + record.lastName)}">Delete</button>`
                : ""}
            </div></td>
          </tr>`).join("")}</tbody>
      </table>
    </div>`;
}

function bindRecordActionButtons(container) {
  container.querySelectorAll("[data-delete-record]").forEach((button) => {
    button.addEventListener("click", () => openDeleteConfirmation(
      Number(button.dataset.deleteRecord),
      button.dataset.recordName
    ));
  });
  container.querySelectorAll("[data-edit-record]").forEach((button) => {
    button.addEventListener("click", () =>
      openRecordEditor(Number(button.dataset.editRecord)));
  });
  container.querySelectorAll("[data-archive-record]").forEach((button) => {
    button.addEventListener("click", () =>
      archiveRecord(Number(button.dataset.archiveRecord)));
  });
}

async function loadRecords() {
  const status = document.querySelector("#record-status").value;
  try {
    const data = await api(`/api/records?status=${encodeURIComponent(status)}`);
    const container = document.querySelector("#records-content");
    container.innerHTML = recordTable(data.items);
    bindRecordActionButtons(container);
  } catch (error) {
    showPageError(error.message);
  }
}

async function loadSearch() {
  document.querySelector("#search-results").innerHTML =
    '<div class="empty-state">Enter a name or record ID and choose filters to search.</div>';
}

async function loadReport() {
  await generateReport();
}

async function generateReport() {
  const from = document.querySelector("#report-from").value;
  const to = document.querySelector("#report-to").value;
  try {
    const report = await api(`/api/reports?from=${encodeURIComponent(from)}&to=${encodeURIComponent(to)}`);
    const metrics = report.metrics;
    document.querySelector("#report-results").innerHTML = `
      <div class="metrics-grid">
        <article class="metric-card"><span>Created in range</span><strong>${metrics.totalCreated}</strong></article>
        <article class="metric-card"><span>Active</span><strong>${metrics.active}</strong></article>
        <article class="metric-card"><span>Archived</span><strong>${metrics.archived}</strong></article>
      </div>`;
  } catch (error) {
    showPageError(error.message);
  }
}

async function loadUsers() {
  if (currentUser?.role !== "Administrator") {
    showPageError("User management is available only to administrators.");
    return;
  }
  try {
    const data = await api("/api/users");
    document.querySelector("#users-content").innerHTML = `
      <div class="table-wrap"><table>
        <thead><tr><th>Name</th><th>Username</th><th>Role</th><th>Status</th></tr></thead>
        <tbody>${data.items.map((user) => `
          <tr><td>${escapeHtml(user.name)}</td><td>${escapeHtml(user.username)}</td>
          <td>${escapeHtml(user.role)}</td>
          <td>${user.active ? "Active" : "Inactive"}</td></tr>`).join("")}</tbody>
      </table></div>`;
  } catch (error) {
    showPageError(error.message);
  }
}

async function openRecordEditor(id = null) {
  document.querySelector("#record-form").reset();
  document.querySelector("#record-form-error").hidden = true;
  document.querySelector("#record-id").value = "";
  document.querySelector("#record-gender").value = "Not specified";
  document.querySelector("#record-dialog-title").textContent =
    id ? "Edit record" : "Add record";
  if (id) {
    try {
      const record = await api(`/api/records/${id}`);
      document.querySelector("#record-id").value = record.id;
      document.querySelector("#record-first-name").value = record.firstName;
      document.querySelector("#record-last-name").value = record.lastName;
      document.querySelector("#record-dob").value = record.dateOfBirth || "";
      document.querySelector("#record-gender").value =
        record.gender || "Not specified";
      document.querySelector("#record-notes").value = record.notes || "";
    } catch (error) {
      showPageError(error.message);
      return;
    }
  }
  recordDialog.showModal();
}

async function archiveRecord(id) {
  if (!window.confirm("Archive this record? The record will remain available in reports and search."))
    return;
  try {
    await api(`/api/records/${id}/archive`, {method: "POST"});
    showToast("Record archived.");
    await loadRecords();
  } catch (error) {
    showPageError(error.message);
  }
}

function openDeleteConfirmation(id, name) {
  pendingDeleteId = id;
  document.querySelector("#delete-record-description").textContent =
    `Permanently delete ${name || "this record"}? This action cannot be undone.`;
  deleteDialog.showModal();
}

async function deleteRecord() {
  if (!pendingDeleteId) return;
  const confirmButton = document.querySelector("#confirm-delete-button");
  confirmButton.disabled = true;
  confirmButton.textContent = "Deleting...";
  try {
    await api(`/api/records/${pendingDeleteId}`, {method: "DELETE"});
    deleteDialog.close();
    pendingDeleteId = null;
    showToast("Record deleted permanently.");
    await loadRecords();
  } catch (error) {
    showToast(error.message);
  } finally {
    confirmButton.disabled = false;
    confirmButton.textContent = "Delete permanently";
  }
}

async function authenticate(username, password) {
  const data = await api("/api/auth/login", {
    method: "POST",
    body: JSON.stringify({username, password})
  });
  token = data.token;
  currentUser = data.user;
  localStorage.setItem("ors_token", token);
  loginView.hidden = true;
  appView.hidden = false;
  document.querySelector("#staff-name").textContent =
    `${currentUser.name} · ${currentUser.role}`;
  document.querySelectorAll(".admin-only").forEach((item) => {
    item.hidden = currentUser.role !== "Administrator";
  });
  showView("dashboard");
}

async function restoreSession() {
  if (!token) return;
  try {
    currentUser = await api("/api/auth/me");
    loginView.hidden = true;
    appView.hidden = false;
    document.querySelector("#staff-name").textContent =
      `${currentUser.name} · ${currentUser.role}`;
    document.querySelectorAll(".admin-only").forEach((item) => {
      item.hidden = currentUser.role !== "Administrator";
    });
    showView("dashboard");
  } catch {
    token = null;
    localStorage.removeItem("ors_token");
  }
}

document.querySelector("#login-form").addEventListener("submit", async (event) => {
  event.preventDefault();
  const username = document.querySelector("#username").value.trim();
  const password = document.querySelector("#password").value;
  const error = document.querySelector("#login-error");
  if (!username || !password) {
    error.hidden = false;
    error.textContent = "Enter both your username and password.";
    return;
  }
  try {
    await authenticate(username, password);
  } catch (caught) {
    error.hidden = false;
    error.textContent = caught.message;
  }
});

document.querySelector("#logout-button").addEventListener("click", async () => {
  try { await api("/api/auth/logout", {method: "POST"}); } catch {}
  token = null;
  currentUser = null;
  localStorage.removeItem("ors_token");
  appView.hidden = true;
  loginView.hidden = false;
  document.querySelector("#password").value = "";
});

document.querySelectorAll(".nav-button").forEach((button) => {
  button.addEventListener("click", () => showView(button.dataset.view));
});

document.querySelector("#menu-button").addEventListener("click", (event) => {
  const sidebar = document.querySelector(".sidebar");
  sidebar.classList.toggle("open");
  event.currentTarget.setAttribute("aria-expanded",
    sidebar.classList.contains("open") ? "true" : "false");
});

document.querySelector("#record-status").addEventListener("change", loadRecords);
document.querySelector("#add-record-button").addEventListener("click",
  () => openRecordEditor());

document.querySelector("#search-form").addEventListener("submit", async (event) => {
  event.preventDefault();
  const query = document.querySelector("#search-query").value.trim();
  const status = document.querySelector("#search-status").value;
  const gender = document.querySelector("#search-gender").value;
  try {
    const data = await api(
      `/api/records?q=${encodeURIComponent(query)}&status=${encodeURIComponent(status)}&gender=${encodeURIComponent(gender)}`
    );
    const container = document.querySelector("#search-results");
    container.innerHTML = recordTable(data.items, true);
    bindRecordActionButtons(container);
  } catch (error) {
    showPageError(error.message);
  }
});

document.querySelector("#report-form").addEventListener("submit", (event) => {
  event.preventDefault();
  generateReport();
});

document.querySelector("#record-form").addEventListener("submit", async (event) => {
  event.preventDefault();
  const id = document.querySelector("#record-id").value;
  const body = {
    firstName: document.querySelector("#record-first-name").value.trim(),
    lastName: document.querySelector("#record-last-name").value.trim(),
    dateOfBirth: document.querySelector("#record-dob").value,
    gender: document.querySelector("#record-gender").value,
    notes: document.querySelector("#record-notes").value.trim()
  };
  if (!body.firstName || !body.lastName) {
    const error = document.querySelector("#record-form-error");
    error.hidden = false;
    error.textContent = "First name and last name are required.";
    return;
  }
  try {
    await api(id ? `/api/records/${id}` : "/api/records", {
      method: id ? "PUT" : "POST",
      body: JSON.stringify(body)
    });
    recordDialog.close();
    await loadRecords();
  } catch (error) {
    const box = document.querySelector("#record-form-error");
    box.hidden = false;
    box.textContent = error.message;
  }
});

document.querySelectorAll("[data-close-dialog]").forEach((button) => {
  button.addEventListener("click", () => recordDialog.close());
});

document.querySelector("#add-user-button").addEventListener("click", () => {
  document.querySelector("#user-form").reset();
  document.querySelector("#user-form-error").hidden = true;
  userDialog.showModal();
});

document.querySelectorAll("[data-close-user-dialog]").forEach((button) => {
  button.addEventListener("click", () => userDialog.close());
});

document.querySelector("#user-form").addEventListener("submit", async (event) => {
  event.preventDefault();
  const body = {
    name: document.querySelector("#user-name").value.trim(),
    username: document.querySelector("#user-username").value.trim(),
    password: document.querySelector("#user-password").value,
    role: document.querySelector("#user-role").value
  };
  if (body.password.length < 8) {
    const error = document.querySelector("#user-form-error");
    error.hidden = false;
    error.textContent = "Password must contain at least 8 characters.";
    return;
  }
  try {
    await api("/api/users", {method: "POST", body: JSON.stringify(body)});
    userDialog.close();
    await loadUsers();
  } catch (error) {
    const box = document.querySelector("#user-form-error");
    box.hidden = false;
    box.textContent = error.message;
  }
});

document.querySelector("#cancel-delete-button").addEventListener("click", () => {
  pendingDeleteId = null;
  deleteDialog.close();
});

document.querySelector("#confirm-delete-button").addEventListener("click",
  deleteRecord);

restoreSession();
