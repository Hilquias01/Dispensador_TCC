const $ = (id) => document.getElementById(id);

function setMsg(text, ok=null){
  const el = $("msg");
  el.textContent = text || "";
  if(ok === true) el.style.borderColor = "rgba(37,211,102,.55)";
  else if(ok === false) el.style.borderColor = "rgba(255,77,77,.55)";
  else el.style.borderColor = "rgba(255,255,255,.18)";
}

async function loadStatus(){
  const r = await fetch("/api/status");
  const j = await r.json();
  $("wifi").textContent = j.wifi;
  $("ssid").textContent = j.ssid || "-";
  $("ip").textContent = j.ip || "-";
  $("rssi").textContent = (j.rssi ?? "-");
}

async function loadConfig(){
  const r = await fetch("/api/config");
  const j = await r.json();
  $("phone").value = j.whatsappPhone || "";
  setMsg("Config carregada. API Key salva aparece mascarada: " + (j.callmebotKeyMasked || ""), null);
}

async function saveConfig(){
  const phone = $("phone").value.trim();
  const key = $("key").value.trim();

  if(phone.length < 10 || key.length < 5){
    setMsg("Preencha telefone e API Key válidos.", false);
    return;
  }

  setMsg("Salvando...", null);
  const r = await fetch("/api/config", {
    method:"POST",
    headers:{ "Content-Type":"application/json" },
    body: JSON.stringify({ whatsappPhone: phone, callmebotKey: key })
  });

  const ok = r.ok;
  setMsg(ok ? "✅ Salvo com sucesso!" : "❌ Erro ao salvar (verifique os dados).", ok);
  $("key").value = ""; // limpa por segurança
}

async function testWhatsApp(){
  setMsg("Enviando teste...", null);
  const r = await fetch("/api/test-whatsapp");
  const j = await r.json();
  setMsg(j.ok ? "✅ Teste enviado no WhatsApp!" : "❌ Falhou. Confira telefone/key e se o CallMeBot está ativo.", j.ok);
}

$("btnSave").addEventListener("click", saveConfig);
$("btnTest").addEventListener("click", testWhatsApp);
$("btnRefresh").addEventListener("click", async ()=> {
  await loadStatus();
  await loadConfig();
});

(async function init(){
  await loadStatus();
  await loadConfig();
})();
