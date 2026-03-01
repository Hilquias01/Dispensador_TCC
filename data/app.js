// Dispensador TCC - V5 (Multi Horários + Fix Wi-Fi)

const $ = (id) => document.getElementById(id);

const el = {
  alarmBanner: $("alarmBanner"),
  stWifi: $("stWifi"), stIp: $("stIp"),
  clockDisplay: $("clockDisplay"), medList: $("medList"),
  tgUsers: $("tgUsers"),
  mName: $("mName"), mDose: $("mDose"),
  mBox: $("mBox"), mBefore: $("mBefore"),
  timeList: $("timeList")
};

let localMeds = []; 

async function apiGet(url) {
  try {
    const r = await fetch(url);
    if (!r.ok) throw new Error(r.status);
    return await r.json();
  } catch (e) { console.error(e); return null; }
}

async function apiPost(url, data) {
  try {
    const r = await fetch(url, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(data)
    });
    return r.ok;
  } catch (e) { console.error(e); return false; }
}

async function refreshStatus() {
  const s = await apiGet("/api/status");
  if (s) {
    if(el.clockDisplay) el.clockDisplay.textContent = s.time || "--:--";
    if(el.stWifi) el.stWifi.textContent = s.wifi ? "Conectado: " + s.ssid : "Off";
    if(el.stIp) el.stIp.textContent = s.ip || "-";
    
    $("chipOnline").style.display = "flex";
    $("dotOnline").className = "dot ok";

    if (s.alarm) el.alarmBanner.classList.remove("hidden");
    else el.alarmBanner.classList.add("hidden");
  } else {
    $("dotOnline").className = "dot bad";
  }
}

$("btnStopAlarm").onclick = async () => {
  await apiPost("/api/stop-alarm", {});
  el.alarmBanner.classList.add("hidden");
};

async function loadConfig() {
  const c = await apiGet("/api/config");
  if (c && el.tgUsers) el.tgUsers.value = c.telegramUsers || "";
}

$("btnSaveCfg").onclick = async () => {
  const data = { telegramUsers: el.tgUsers.value, html: false };
  const ok = await apiPost("/api/config", data);
  alert(ok ? "Salvo!" : "Erro.");
};

$("btnTestTg").onclick = async () => {
  alert("Enviando teste...");
  await apiGet("/api/test-telegram");
};

// --- Lógica Multi-Horários ---
$("btnAddTime").onclick = () => {
  const input = document.createElement("input");
  input.type = "time";
  input.className = "time-input";
  input.style.marginTop = "5px"; 
  el.timeList.appendChild(input);
};

async function loadMeds() {
  el.medList.innerHTML = '<div class="small muted">Buscando...</div>';
  const data = await apiGet("/api/meds");
  if (Array.isArray(data)) {
    localMeds = data;
    renderMeds();
  } else {
    el.medList.innerHTML = '<div class="small muted">Erro ao carregar lista.</div>';
  }
}

function renderMeds() {
  if (localMeds.length === 0) {
    el.medList.innerHTML = '<div class="small muted">Nenhum agendamento.</div>';
    return;
  }
  
  const daysLabel = ["Dom","Seg","Ter","Qua","Qui","Sex","Sáb"];
  localMeds.sort((a,b) => a.time.localeCompare(b.time));

  el.medList.innerHTML = localMeds.map((m, i) => {
    const dStr = m.days.map(d => daysLabel[d]).join(", ");
    return `
      <div class="meditem">
        <div class="medtop">
          <div>
            <div class="medname">${m.name} <span class="badge">${m.time}</span></div>
            <div class="medmeta">
              <b>📦 COMPARTIMENTO ${m.box || 1}</b> • ${m.dose} <br> 
              ${dStr}
            </div>
          </div>
          <button class="btn danger small" style="width:auto" onclick="delMed(${i})">X</button>
        </div>
      </div>
    `;
  }).join("");
}

$("btnAddMed").onclick = async () => {
  const name = el.mName.value.trim();
  const timeInputs = document.querySelectorAll(".time-input");
  const times = [];
  timeInputs.forEach(i => { if(i.value) times.push(i.value); });

  if (!name || times.length === 0) return alert("Preencha Nome e pelo menos um Horário!");

  const days = [];
  for(let i=0; i<=6; i++) if($("d"+i).checked) days.push(i);
  if (days.length === 0) return alert("Escolha pelo menos um dia.");

  const box = parseInt(el.mBox.value) || 1;
  const dose = el.mDose.value.trim();
  const beforeMin = parseInt(el.mBefore.value) || 5;

  times.forEach(t => {
    const newMed = {
      id: Date.now().toString() + Math.random(), 
      name, dose, 
      time: t, 
      box, beforeMin, days, active: true
    };
    localMeds.push(newMed);
  });

  await apiPost("/api/meds", localMeds);
  loadMeds();
  alert("Agendados " + times.length + " horários!");
  
  el.mName.value = "";
  el.timeList.innerHTML = '<input type="time" class="time-input" required>';
};

window.delMed = async (index) => {
  if(confirm("Apagar?")) {
    localMeds.splice(index, 1);
    await apiPost("/api/meds", localMeds);
    loadMeds();
  }
};

$("btnReloadMeds").onclick = loadMeds;

function init() {
  document.querySelectorAll(".tab").forEach(t => {
    t.onclick = () => {
      document.querySelectorAll(".tab").forEach(x => x.classList.remove("active"));
      document.querySelectorAll(".panel").forEach(x => x.classList.add("hidden"));
      t.classList.add("active");
      $( "tab-" + t.dataset.tab ).classList.remove("hidden");
    };
  });

  loadConfig();
  loadMeds();
  refreshStatus();
  setInterval(refreshStatus, 3000);
}

init();