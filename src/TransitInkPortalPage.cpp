#include "TransitInkPortalPage.h"
#include "ProductConfig.h"

const char kTransitInkPortalHtml[] PROGMEM = R"HTML(
<!doctype html>
<html lang="zh-Hant-HK">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>)HTML" FIRMWARE_PRODUCT_NAME R"HTML( 裝置設定</title>
<style>
:root{color-scheme:light dark;--bg:#f4f6f9;--surface:#fff;--surface-2:#edf1f7;--text:#142033;--muted:#526176;--border:#c7d0dc;--accent:#1558d6;--accent-hover:#1047ad;--accent-text:#fff;--danger:#a31d2d;--focus:#0a66ff;--radius:12px;--shadow:0 8px 24px rgba(20,32,51,.09)}
*{box-sizing:border-box}
html{background:var(--bg);color:var(--text);font-family:-apple-system,BlinkMacSystemFont,"PingFang HK","Noto Sans CJK TC",sans-serif;line-height:1.45}
body{margin:0;min-width:280px;padding-bottom:94px}
button,input,select{font:inherit;color:inherit}
button{cursor:pointer}
button:disabled{cursor:not-allowed;opacity:.46}
button:active:not(:disabled){transform:translateY(1px)}
button,input,select{border-radius:var(--radius)}
button:focus-visible,input:focus-visible,select:focus-visible{outline:3px solid var(--focus);outline-offset:2px}
.shell{width:min(860px,100%);margin:0 auto;padding:24px 18px 34px}
.masthead{display:flex;justify-content:space-between;align-items:flex-end;gap:20px;margin-bottom:20px}
.brand{font-size:28px;line-height:1.1;margin:0 0 5px;letter-spacing:-.02em}.context{margin:0;color:var(--muted)}
.top-facts{margin:0;text-align:right;color:var(--muted);font-size:14px}
.tabs{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:6px;padding:6px;background:var(--surface-2);border:1px solid var(--border);border-radius:var(--radius);margin-bottom:18px}
.tab{min-height:44px;padding:8px 10px;border:0;background:transparent;color:var(--muted);font-weight:650;white-space:nowrap}
.tab[aria-selected="true"]{background:var(--surface);color:var(--accent);box-shadow:var(--shadow)}
.panel[hidden]{display:none}.panel{display:grid;gap:14px}
.section{background:var(--surface);border:1px solid var(--border);border-radius:var(--radius);padding:20px}
h2{font-size:20px;margin:0 0 5px}h3{font-size:17px;margin:0}.intro,.helper{color:var(--muted);margin:0}.helper{font-size:13px}
.field-grid{display:grid;grid-template-columns:1fr 1fr;gap:14px;margin-top:16px}.field{display:grid;gap:6px}.field.full{grid-column:1/-1}
label,.label{font-size:14px;font-weight:650}input,select{width:100%;min-height:44px;padding:10px 12px;border:1px solid var(--border);background:var(--surface);color:var(--text)}
select:disabled,input:disabled{opacity:.68;cursor:not-allowed}.catalog-select[aria-busy="true"],.catalog-search[aria-busy="true"]{cursor:wait}.catalog-search[aria-invalid="true"]{border-color:var(--danger)}.catalog-status{display:flex;align-items:center;gap:7px;color:var(--muted);font-size:13px}.catalog-status::before{content:"";width:14px;height:14px;border:2px solid var(--border);border-top-color:var(--accent);border-radius:50%;animation:catalog-spin .8s linear infinite}@keyframes catalog-spin{to{transform:rotate(360deg)}}
.readonly-grid,.fact-grid,.cadence-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:1px;background:var(--border);border:1px solid var(--border);border-radius:var(--radius);overflow:hidden;margin-top:15px}
.read-row,.fact-row,.cadence-row{background:var(--surface-2);padding:12px 14px;display:grid;gap:3px}.read-row span,.fact-row span,.cadence-row span{color:var(--muted);font-size:13px}.read-row strong,.fact-row strong,.cadence-row strong{font-size:15px}
.secondary{min-height:42px;padding:9px 14px;border:1px solid var(--border);background:var(--surface);color:var(--accent);font-weight:650}.secondary:hover{border-color:var(--accent)}
.wifi-actions{display:flex;align-items:center;gap:10px;margin-top:14px}.wifi-actions .helper{flex:1}
.catalog-actions{display:flex;align-items:center;gap:12px;margin-top:14px}.catalog-actions .helper{flex:1}.catalog-actions button[aria-busy="true"]::before{content:"";display:inline-block;width:14px;height:14px;margin-right:8px;border:2px solid currentColor;border-right-color:transparent;border-radius:50%;vertical-align:-2px;animation:catalog-spin .8s linear infinite}
.widget-list{display:grid;gap:10px}.widget-card{background:var(--surface);border:1px solid var(--border);border-radius:var(--radius);overflow:hidden}.widget-card[data-expanded="true"]{border-color:var(--accent);box-shadow:var(--shadow)}
.widget-head{display:grid;grid-template-columns:minmax(0,1fr) auto;gap:12px;padding:14px;align-items:center}.widget-toggle{border:0;background:transparent;padding:0;text-align:left;min-width:0}.widget-title{display:block;font-weight:750}.widget-summary{display:block;color:var(--muted);font-size:13px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;margin-top:2px}.order-actions{display:flex;gap:6px}.order-actions button{min-height:36px;padding:7px 9px}
.widget-body{border-top:1px solid var(--border);padding:16px;background:var(--surface-2)}.widget-controls{display:grid;grid-template-columns:1fr 1fr;gap:12px}.widget-preview{margin:14px 0 0;padding:11px 12px;background:var(--surface);border-radius:var(--radius);color:var(--muted);font-size:14px}.widget-error{min-height:20px;margin:8px 0 0;color:var(--danger);font-size:13px;font-weight:650}
.toggle-field{display:flex;align-items:center;gap:10px;min-height:44px}.toggle-field input{width:20px;min-height:20px;margin:0}
.action-bar{position:fixed;z-index:20;left:0;right:0;bottom:0;background:var(--surface);border-top:1px solid var(--border);box-shadow:0 -7px 22px rgba(20,32,51,.09)}.action-inner{width:min(860px,100%);margin:0 auto;padding:12px 18px;display:flex;align-items:center;justify-content:space-between;gap:18px}.status{margin:0;color:var(--muted);font-size:14px;white-space:pre-wrap}.primary{min-height:46px;padding:11px 18px;border:1px solid var(--accent);background:var(--accent);color:var(--accent-text);font-weight:750;white-space:nowrap}.primary:hover{background:var(--accent-hover);border-color:var(--accent-hover)}
@media(max-width:640px){body{padding-bottom:122px}.shell{padding:16px 12px 28px}.masthead{display:block}.top-facts{text-align:left;margin-top:9px}.tab{font-size:12px;padding-inline:4px}.section{padding:16px}.field-grid,.widget-controls,.readonly-grid,.fact-grid,.cadence-grid{grid-template-columns:1fr}.field.full{grid-column:auto}.widget-head{grid-template-columns:1fr}.order-actions{justify-content:flex-start}.action-inner{display:grid;gap:8px;padding:10px 12px}.primary{width:100%}}
@media(prefers-color-scheme:dark){:root{--bg:#0f1724;--surface:#182233;--surface-2:#202d40;--text:#f2f6fc;--muted:#afbed2;--border:#425168;--accent:#6ea2ff;--accent-hover:#8cb5ff;--accent-text:#071426;--danger:#ff8c9a;--focus:#8cb5ff;--shadow:0 8px 24px rgba(0,0,0,.24)}}
@media(prefers-reduced-motion:reduce){*,*::before,*::after{scroll-behavior:auto!important;transition:none!important;animation:none!important}}
</style>
</head>
<body>
<form id="config_form" novalidate>
<main class="shell">
<header class="masthead">
<div><h1 class="brand">)HTML" FIRMWARE_PRODUCT_NAME R"HTML(</h1><p class="context">裝置設定</p></div>
<p class="top-facts" id="connection_summary" aria-live="polite">正在讀取裝置資料</p>
</header>
<nav class="tabs" role="tablist" aria-label="設定分類">
<button class="tab" id="tab_wifi" role="tab" aria-selected="true" aria-controls="panel_wifi" type="button" onclick="selectTab('wifi')">Wi-Fi</button>
<button class="tab" id="tab_widgets" role="tab" aria-selected="false" aria-controls="panel_widgets" type="button" onclick="selectTab('widgets')">主頁小工具</button>
<button class="tab" id="tab_power" role="tab" aria-selected="false" aria-controls="panel_power" type="button" onclick="selectTab('power')">更新及省電</button>
</nav>

<section class="panel" id="panel_wifi" role="tabpanel" aria-labelledby="tab_wifi">
<div class="section"><h2>Wi-Fi 連線</h2><p class="intro">選擇附近網絡，或直接輸入 Wi-Fi 名稱。</p>
<div class="readonly-grid"><div class="read-row"><span>目前網絡</span><strong id="current_ssid">尚未設定</strong></div><div class="read-row"><span>密碼狀態</span><strong id="password_status">尚未儲存密碼</strong></div></div>
<div class="wifi-actions"><button class="secondary" type="button" onclick="scanWifi()">掃描附近網絡</button><p class="helper" id="wifi_scan_status">尚未掃描</p></div>
<div class="field-grid"><div class="field full"><label for="wifi_network">附近網絡</label><select id="wifi_network" onchange="selectWifiNetwork(this.value)"><option value="">請先掃描或手動輸入</option></select></div>
<div class="field"><label for="wifi_ssid">Wi-Fi 名稱</label><input id="wifi_ssid" maxlength="32" autocomplete="username" required></div>
<div class="field"><label for="wifi_password">Wi-Fi 密碼</label><input id="wifi_password" type="password" maxlength="64" autocomplete="new-password" aria-describedby="wifi_password_help"><p class="helper" id="wifi_password_help">留空會保留目前已儲存的密碼。</p></div></div></div>
</section>

<section class="panel" id="panel_widgets" role="tabpanel" aria-labelledby="tab_widgets" hidden>
<div class="section"><h2>主頁小工具</h2><p class="intro">四個位置會依次顯示在主頁。類型可以重複，亦可停用。</p></div>
<div class="widget-list" id="widget_cards"></div>
</section>

<section class="panel" id="panel_power" role="tabpanel" aria-labelledby="tab_power" hidden>
<div class="section"><h2>更新設定</h2><p class="intro">交通資料會按固定頻率自動更新。</p><div class="cadence-grid"><div class="cadence-row"><span>港鐵 ETA</span><strong>每 30 秒</strong></div><div class="cadence-row"><span>巴士 ETA</span><strong>每 60 秒</strong></div><div class="cadence-row"><span>專線小巴 ETA</span><strong>每 60 秒</strong></div><div class="cadence-row"><span>行車時間</span><strong>每 120 秒</strong></div></div>
<div class="field-grid"><div class="field full"><label for="weather_location">天氣位置</label><select id="weather_location"></select></div></div></div>
<div class="section"><h2>交通目錄</h2><p class="intro">巴士、小巴、港鐵及輕鐵目錄已包含在韌體內，可離線搜尋。若找不到新路線或站牌，可由裝置重新取得完整巴士及小巴路線索引；目前小工具使用的站牌會一併更新，其餘路線首次選用時才下載一次並保存。</p><div class="fact-grid"><div class="fact-row"><span>基線來源</span><strong id="catalog_source">韌體內建</strong></div><div class="fact-row"><span>基線版本</span><strong id="catalog_revision">讀取中</strong></div><div class="fact-row"><span>基線大小</span><strong id="catalog_size">讀取中</strong></div><div class="fact-row"><span>上次路線更新</span><strong id="catalog_last_checked">尚未更新</strong></div></div><div class="catalog-actions"><button class="secondary" id="catalog_update_button" type="button" onclick="refreshAllRoutes()">找不到站牌？更新所有路線</button><p class="helper" id="catalog_update_status" role="status" aria-live="polite">更新資料會保存於裝置 LittleFS。</p></div></div>
<div class="section"><h2>省電</h2><div class="field-grid"><div class="field full"><span class="label">省電睡眠模式</span><label class="toggle-field" for="sleep_enabled"><input id="sleep_enabled" type="checkbox">啟用定時休眠</label></div><div class="field"><label for="wake_duration_minutes">醒著時間（分鐘）</label><input id="wake_duration_minutes" type="number" min="1" max="60" inputmode="numeric"></div><div class="field"><label for="sleep_maintenance_hours">睡眠中時間及天氣同步（小時）</label><input id="sleep_maintenance_hours" type="number" min="0" max="24" inputmode="numeric"><p class="helper">不會喚醒畫面或更新即時交通資料；輸入 0 代表停用。</p></div></div></div>
<div class="section"><h2>裝置資料</h2><div class="fact-grid"><div class="fact-row"><span>韌體版本</span><strong id="firmware_version">讀取中</strong></div><div class="fact-row"><span>電量</span><strong id="battery_percent">讀取中</strong></div><div class="fact-row"><span>供電狀態</span><strong id="battery_state">讀取中</strong></div><div class="fact-row"><span>裝置</span><strong>)HTML" FIRMWARE_SHORT_NAME R"HTML(</strong></div></div></div>
</section>
</main>
<footer class="action-bar"><div class="action-inner"><p class="status" id="save_status" role="status" aria-live="polite">尚未儲存變更</p><button class="primary" type="submit">儲存並重新啟動</button></div></footer>
</form>
<script>
const weatherLocations=['香港天文台','京士柏','黃竹坑','打鼓嶺','流浮山','大埔','沙田','屯門','將軍澳','西貢','長洲','赤鱲角','青衣','荃灣可觀','荃灣城門谷','香港公園','筲箕灣','九龍城','跑馬地','黃大仙','赤柱','觀塘','深水埗','啟德跑道公園','元朗公園','大美督'];
function emptyWidget(){return{type:'disabled',bus:{operator:'kmb',route_id:'',direction_id:'',service_type:'',stop_id:'',route_label_tc:'',stop_label_tc:'',destination_label_tc:''},gmb:{region:'',route_code:'',route_id:'',route_seq:'',stop_id:'',stop_seq:'',route_label_tc:'',stop_label_tc:'',direction_label_tc:''},mtr:{mode:'heavy_rail',line_or_route_id:'',station_id:'',direction_id:'',line_or_route_label_tc:'',station_label_tc:'',direction_label_tc:''},journey_time:{location_id:'',destination_id:'',location_label_tc:'',destination_label_tc:''}}}
let savedConfig=null;
let csrfToken='';
const csrfHeader='X-TransitInk-CSRF';
const accessHeader='X-TransitInk-Access';
const portalAccessToken=typeof location==='undefined'?'':location.pathname.split('/').filter(Boolean)[0]||'';
const localCatalog={index:null,rail:null,routeIndex:null,providers:{},promises:{},overrides:new Map(),overridePromises:new Map()};
let widgetDrafts=Array.from({length:4},()=>emptyWidget());
let expandedSlot=0;
const requestVersion=[0,0,0,0];
const catalogKeys=['busRoutes','busDirections','busStops','gmbRoutes','gmbDirections','gmbStops','railLines','railStations','railDirections','journeyLocations','journeyDestinations'];
function emptyCatalogEntry(){return{status:'idle',items:[],error:''}}
function emptyCatalogState(){return Object.fromEntries(catalogKeys.map(key=>[key,emptyCatalogEntry()]))}
const catalogState=Array.from({length:4},()=>emptyCatalogState());
const widgetErrors=['','','',''];
let catalogUpdateState='idle';
let firstWidgetValidation=null;
function byId(id){return document.getElementById(id)}
function escapeHtml(value){return String(value??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]))}
function portalFetch(path,options={}){const headers={...(options.headers||{})};if(portalAccessToken)headers[accessHeader]=portalAccessToken;return fetch(path,{...options,headers})}
function api(path,options){return portalFetch(path,options).then(async response=>{if(!response.ok)throw new Error(await response.text());return response.json()})}
function loadedEntry(items){return{status:'loaded',items:Array.isArray(items)?items:[],error:''}}
function providerKey(operator){return operator==='ctb'?'ctb':'kmb'}
function routeItems(routes){return Object.keys(routes||{}).sort((a,b)=>a.localeCompare(b,undefined,{numeric:true})).map(id=>({id,label_tc:id}))}
function mergeRouteItems(primary,secondary){const merged=new Map();[...(primary||[]),...(secondary||[])].forEach(item=>{if(item?.id)merged.set(String(item.id).toUpperCase(),{id:String(item.id).toUpperCase(),label_tc:item.label_tc||item.id})});return[...merged.values()].sort((a,b)=>a.id.localeCompare(b.id,undefined,{numeric:true}))}
function updatedBusRoutes(operator){return localCatalog.routeIndex?.bus?.[providerKey(operator)]||[]}
function busRouteItems(operator){const provider=providerKey(operator),embedded=routeItems(localCatalog.index?.bus?.[provider==='kmb'?'kmb_lwb':'ctb']?.routes);return mergeRouteItems(updatedBusRoutes(operator),embedded)}
function gmbRouteItems(){return mergeRouteItems(localCatalog.routeIndex?.gmb,routeItems(localCatalog.index?.gmb?.routes))}
function overrideKey(kind,operator,route){return`${kind}:${operator||''}:${String(route||'').toUpperCase()}`}
function routeOverride(kind,operator,route){return localCatalog.overrides.get(overrideKey(kind,operator,route))}
function busDirectionItems(operator,route){const override=routeOverride('bus',operator,route);if(override)return override.directions||[];const key=providerKey(operator),directions=localCatalog.index?.bus?.[key==='kmb'?'kmb_lwb':'ctb']?.routes?.[route]||[];return directions.map(item=>({id:`${item.direction_id}:${item.service_type}`,direction_id:item.direction_id,service_type:item.service_type,label_tc:`${item.origin_label_tc} 往 ${item.destination_label_tc}${item.service_type==='1'?'':`（服務 ${item.service_type}）`}`,origin_label_tc:item.origin_label_tc,destination_label_tc:item.destination_label_tc,stop_key:item.stop_key}))}
function gmbDirectionItems(route){const override=routeOverride('gmb','',route);if(override)return override.directions||[];return(localCatalog.index?.gmb?.routes?.[route]||[]).map(item=>({...item,label_tc:`${item.origin_label_tc} 往 ${item.destination_label_tc}`}))}
function railGroups(mode){return localCatalog.rail?.modes?.[mode]||[]}
function activeRailGroup(widget){return railGroups(widget.mtr.mode).find(item=>item.id===widget.mtr.line_or_route_id)}
function assertLocalCatalog(index,rail){if(index?.schema_version!==1||rail?.schema_version!==1||!index.revision||index.revision!==rail.revision)throw new Error('本地交通目錄版本不一致');if(!index.bus?.kmb_lwb?.routes||!index.bus?.ctb?.routes||!index.gmb?.routes||!rail.modes)throw new Error('本地交通目錄格式不正確')}
async function loadLocalCatalog(){const index=await api('/assets/catalog/current/index.json',{cache:'no-cache'}),rail=await api(`/assets/catalog/current/rail.json?revision=${encodeURIComponent(index.revision||'')}`);assertLocalCatalog(index,rail);localCatalog.index=index;localCatalog.rail=rail}
async function loadUpdatedRouteIndex(){const response=await portalFetch('/api/catalog/route-index',{cache:'no-store'});if(response.status===404)return null;if(!response.ok)throw new Error(await response.text());const result=await response.json();if(!result?.bus?.kmb||!result?.bus?.ctb||!result?.gmb)throw new Error('本機更新路線索引格式不正確');localCatalog.routeIndex=result;return result}
async function loadProviderCatalog(provider){if(localCatalog.providers[provider])return localCatalog.providers[provider];if(localCatalog.promises[provider])return localCatalog.promises[provider];const revision=localCatalog.index?.revision||'';localCatalog.promises[provider]=api(`/assets/catalog/current/stops-${provider}.json?revision=${encodeURIComponent(revision)}`).then(result=>{if(result?.schema_version!==1||result.revision!==revision||!result.routes)throw new Error(`${provider} 站牌目錄版本不一致`);localCatalog.providers[provider]=result;delete localCatalog.promises[provider];return result}).catch(error=>{delete localCatalog.promises[provider];throw error});return localCatalog.promises[provider]}
function selectTab(name){
  ['wifi','widgets','power'].forEach(tab=>{const selected=tab===name;byId(`tab_${tab}`).setAttribute('aria-selected',String(selected));byId(`panel_${tab}`).hidden=!selected});
  if(name==='widgets')ensureCatalogForSlot(expandedSlot);
}
function typeLabel(type){return{disabled:'停用',bus_eta:'巴士 ETA',gmb_eta:'專線小巴 ETA',mtr_eta:'港鐵 ETA',journey_time:'行車時間'}[type]||'設定不完整'}
function widgetSummary(widget){
  if(widget.type==='disabled')return'不顯示內容';
  if(widget.type==='bus_eta')return[widget.bus.route_label_tc||widget.bus.route_id,widget.bus.stop_label_tc].filter(Boolean).join('，')||'尚未完成巴士設定';
  if(widget.type==='gmb_eta')return[widget.gmb.route_label_tc||widget.gmb.route_code,widget.gmb.stop_label_tc,widget.gmb.direction_label_tc].filter(Boolean).join('，')||'尚未完成專線小巴設定';
  if(widget.type==='mtr_eta')return[widget.mtr.line_or_route_label_tc,widget.mtr.station_label_tc,widget.mtr.direction_label_tc].filter(Boolean).join('，')||'尚未完成鐵路設定';
  return[widget.journey_time.location_label_tc,widget.journey_time.destination_label_tc].filter(Boolean).join('，')||'尚未完成行車時間設定';
}
function selectedOption(value,label){return value&&label?`<option value="${escapeHtml(value)}" selected>${escapeHtml(label)}</option>`:''}
function optionRows(items,value){return(items||[]).map(item=>`<option value="${escapeHtml(item.id)}" ${item.id===value?'selected':''} data-direction="${escapeHtml(item.direction_id||'')}" data-service="${escapeHtml(item.service_type||'')}" data-stop-key="${escapeHtml(item.stop_key||'')}" data-origin="${escapeHtml(item.origin_label_tc||'')}" data-destination="${escapeHtml(item.destination_label_tc||'')}" data-region="${escapeHtml(item.region||'')}" data-route-id="${escapeHtml(item.route_id||'')}" data-route-seq="${escapeHtml(item.route_seq||'')}" data-stop-id="${escapeHtml(item.stop_id||'')}" data-stop-seq="${escapeHtml(item.stop_seq||'')}">${escapeHtml(item.label_tc||item.id)}</option>`).join('')}
function selectField(id,label,source,value,currentLabel,change,placeholder='請選擇'){
  const entry=Array.isArray(source)?{status:'loaded',items:source,error:''}:(source||emptyCatalogEntry()),items=entry.items||[];
  const loading=entry.status==='loading',failed=entry.status==='error',waiting=entry.status==='idle'&&placeholder!=='請選擇',empty=entry.status==='loaded'&&items.length===0;
  const disabled=loading||failed||waiting||empty;
  const prompt=loading?`正在載入${label}…`:failed?'未能載入選項':empty?'暫無可用選項':placeholder;
  const hasCurrent=(items||[]).some(item=>item.id===value);
  const status=loading?`<span class="catalog-status" id="${id}_status" role="status" aria-live="polite">正在載入${label}…</span>`:'';
  return`<div class="field"><label for="${id}">${label}</label><select class="catalog-select" id="${id}" onchange="${change}" aria-busy="${loading}" ${loading?`aria-describedby="${id}_status"`:''} ${disabled?'disabled':''}><option value="">${prompt}</option>${hasCurrent?'':selectedOption(value,currentLabel)}${optionRows(items,value)}</select>${status}</div>`;
}
function searchableRouteField(id,label,source,value,currentLabel,inputHandler,placeholder){
  const entry=source||emptyCatalogEntry(),items=entry.items||[];
  const loading=entry.status==='loading',failed=entry.status==='error',waiting=entry.status==='idle',empty=entry.status==='loaded'&&items.length===0;
  const disabled=loading||failed||waiting||empty;
  const prompt=loading?`正在載入${label}…`:failed?'未能載入路線':empty?'暫無可用路線':placeholder;
  const status=loading?`<span class="catalog-status" id="${id}_status" role="status" aria-live="polite">正在載入${label}…</span>`:'';
  const describedBy=`${id}_help${loading?` ${id}_status`:''}`;
  const options=items.map(item=>`<option value="${escapeHtml(item.id)}">${escapeHtml(item.label_tc||item.id)}</option>`).join('');
  return`<div class="field"><label for="${id}">${label}</label><input class="catalog-search" id="${id}" type="search" list="${id}_options" value="${escapeHtml(currentLabel||value)}" placeholder="${escapeHtml(prompt)}" onchange="${inputHandler}" autocomplete="off" autocapitalize="characters" spellcheck="false" maxlength="16" aria-busy="${loading}" aria-invalid="false" aria-describedby="${describedBy}" ${disabled?'disabled':''}><datalist id="${id}_options">${options}</datalist>${status}<p class="helper" id="${id}_help">輸入路線代號，然後從建議選擇。</p></div>`;
}
function gmbDirectionValue(gmb){return gmb.route_id&&gmb.route_seq?`${gmb.route_id}:${gmb.route_seq}`:''}
function busDirectionValue(bus){return bus.direction_id&&bus.service_type?`${bus.direction_id}:${bus.service_type}`:''}
function renderWidgetFields(slot){
  const widget=widgetDrafts[slot],catalog=catalogState[slot];
  let html=`<div class="field full"><label for="widget_type_${slot}">小工具類型</label><select id="widget_type_${slot}" onchange="setWidgetType(${slot},this.value)"><option value="disabled" ${widget.type==='disabled'?'selected':''}>停用</option><option value="bus_eta" ${widget.type==='bus_eta'?'selected':''}>巴士 ETA</option><option value="gmb_eta" ${widget.type==='gmb_eta'?'selected':''}>專線小巴 ETA</option><option value="mtr_eta" ${widget.type==='mtr_eta'?'selected':''}>港鐵 ETA</option><option value="journey_time" ${widget.type==='journey_time'?'selected':''}>行車時間</option></select></div>`;
  if(widget.type==='bus_eta'){
    html+=selectField(`bus_operator_${slot}`,'營辦商',[{id:'kmb',label_tc:'九巴'},{id:'lwb',label_tc:'龍運'},{id:'ctb',label_tc:'城巴'}],widget.bus.operator,{'kmb':'九巴','lwb':'龍運','ctb':'城巴'}[widget.bus.operator],`setBusOperator(${slot},this.value)`);
    html+=searchableRouteField(`bus_route_${slot}`,'路線',catalog.busRoutes,widget.bus.route_id,widget.bus.route_label_tc,`setBusRouteSearch(${slot},this)`,'輸入路線代號');
    html+=selectField(`bus_direction_${slot}`,'方向及服務',catalog.busDirections,busDirectionValue(widget.bus),widget.bus.destination_label_tc,`setBusDirection(${slot},this)`,'先選路線');
    html+=selectField(`bus_stop_${slot}`,'站牌',catalog.busStops,widget.bus.stop_id,widget.bus.stop_label_tc,`setBusStop(${slot},this)`,'先選方向');
  }else if(widget.type==='gmb_eta'){
    html+=searchableRouteField(`gmb_route_${slot}`,'路線',catalog.gmbRoutes,widget.gmb.route_code,widget.gmb.route_label_tc,`setGmbRouteSearch(${slot},this)`,'輸入路線代號');
    html+=selectField(`gmb_direction_${slot}`,'方向及班次',catalog.gmbDirections,gmbDirectionValue(widget.gmb),widget.gmb.direction_label_tc,`setGmbDirection(${slot},this)`,'先選路線');
    html+=selectField(`gmb_stop_${slot}`,'站點',catalog.gmbStops,widget.gmb.stop_seq,widget.gmb.stop_label_tc,`setGmbStop(${slot},this)`,'先選方向及班次');
  }else if(widget.type==='mtr_eta'){
    html+=selectField(`rail_mode_${slot}`,'鐵路類型',[{id:'heavy_rail',label_tc:'港鐵'},{id:'light_rail',label_tc:'輕鐵'}],widget.mtr.mode,widget.mtr.mode==='light_rail'?'輕鐵':'港鐵',`setRailMode(${slot},this.value)`);
    html+=selectField(`rail_line_${slot}`,'路線',catalog.railLines,widget.mtr.line_or_route_id,widget.mtr.line_or_route_label_tc,`setRailLine(${slot},this)`,'先選鐵路類型');
    html+=selectField(`rail_station_${slot}`,'車站',catalog.railStations,widget.mtr.station_id,widget.mtr.station_label_tc,`setRailStation(${slot},this)`,'先選路線');
    html+=selectField(`rail_direction_${slot}`,'方向',catalog.railDirections,widget.mtr.direction_id,widget.mtr.direction_label_tc,`setRailDirection(${slot},this)`,'先選車站');
  }else if(widget.type==='journey_time'){
    html+=selectField(`journey_location_${slot}`,'指示地點',catalog.journeyLocations,widget.journey_time.location_id,widget.journey_time.location_label_tc,`setJourneyLocation(${slot},this)`);
    html+=selectField(`journey_destination_${slot}`,'行車方向',catalog.journeyDestinations,widget.journey_time.destination_id,widget.journey_time.destination_label_tc,`setJourneyDestination(${slot},this)`,'先選指示地點');
  }
  return html;
}
function renderWidgetCards(){
  byId('widget_cards').innerHTML=widgetDrafts.map((widget,slot)=>{const expanded=slot===expandedSlot;return`<article class="widget-card" data-expanded="${expanded}"><div class="widget-head"><button class="widget-toggle" type="button" onclick="expandWidgetCard(${slot})" aria-expanded="${expanded}" aria-controls="widget_body_${slot}"><span class="widget-title">位置 ${slot+1}　${typeLabel(widget.type)}</span><span class="widget-summary">${escapeHtml(widgetSummary(widget))}</span></button><div class="order-actions"><button class="secondary" type="button" onclick="moveWidget(${slot},-1)" ${slot===0?'disabled':''}>上移</button><button class="secondary" type="button" onclick="moveWidget(${slot},1)" ${slot===3?'disabled':''}>下移</button></div></div>${expanded?`<div class="widget-body" id="widget_body_${slot}"><div class="widget-controls">${renderWidgetFields(slot)}</div><p class="widget-preview">預覽：${escapeHtml(widgetSummary(widget))}</p><p class="widget-error" id="widget_error_${slot}" role="alert">${escapeHtml(widgetErrors[slot])}</p></div>`:''}</article>`}).join('');
}
function expandWidgetCard(slot){expandedSlot=slot;renderWidgetCards();ensureCatalogForSlot(slot)}
function resetCatalog(slot,...keys){keys.forEach(key=>catalogState[slot][key]=emptyCatalogEntry())}
function setWidgetType(slot,type){requestVersion[slot]++;widgetDrafts[slot]=emptyWidget();widgetDrafts[slot].type=type;catalogState[slot]=emptyCatalogState();widgetErrors[slot]='';renderWidgetCards();ensureCatalogForSlot(slot)}
function moveWidget(slot,delta){const target=slot+delta;if(target<0||target>3)return;[widgetDrafts[slot],widgetDrafts[target]]=[widgetDrafts[target],widgetDrafts[slot]];[catalogState[slot],catalogState[target]]=[catalogState[target],catalogState[slot]];[widgetErrors[slot],widgetErrors[target]]=[widgetErrors[target],widgetErrors[slot]];requestVersion.forEach((_,index)=>requestVersion[index]++);expandedSlot=target;renderWidgetCards();ensureCatalogForSlot(target)}
function clearBusAfterOperator(slot){const bus=widgetDrafts[slot].bus;bus.route_id='';bus.route_label_tc='';clearBusAfterRoute(slot)}
function clearBusAfterRoute(slot){const bus=widgetDrafts[slot].bus;bus.direction_id='';bus.service_type='';bus.destination_label_tc='';bus.stop_id='';bus.stop_label_tc='';resetCatalog(slot,'busDirections','busStops')}
function clearGmbAfterRoute(slot){const gmb=widgetDrafts[slot].gmb;gmb.region='';gmb.route_id='';gmb.route_seq='';gmb.direction_label_tc='';gmb.stop_id='';gmb.stop_seq='';gmb.stop_label_tc='';resetCatalog(slot,'gmbDirections','gmbStops')}
function clearRailAfterMode(slot){const mtr=widgetDrafts[slot].mtr;mtr.line_or_route_id='';mtr.line_or_route_label_tc='';mtr.station_id='';mtr.station_label_tc='';mtr.direction_id='';mtr.direction_label_tc='';resetCatalog(slot,'railLines','railStations','railDirections')}
function clearJourneyAfterLocation(slot){const journey=widgetDrafts[slot].journey_time;journey.destination_id='';journey.destination_label_tc='';resetCatalog(slot,'journeyDestinations')}
function setBusOperator(slot,value){requestVersion[slot]++;widgetDrafts[slot].bus.operator=value;resetCatalog(slot,'busRoutes');clearBusAfterOperator(slot);renderWidgetCards();ensureCatalogForSlot(slot)}
function catalogRouteMatch(items,value){const normalized=value.trim().toUpperCase();return normalized?(items||[]).find(item=>String(item.id).toUpperCase()===normalized):null}
function isRouteCode(value){return/^[A-Z0-9]{1,16}$/.test(value)}
function markRouteSearchInput(input,invalid,message){input.setAttribute('aria-invalid',String(invalid));input.setCustomValidity?.(invalid?'路線代號格式不正確':'');const help=byId(`${input.id}_help`);if(help)help.textContent=message||(invalid?'路線代號只可包含英文字母及數字。':'輸入路線代號，然後從建議選擇。')}
function setBusRouteSearch(slot,input){const bus=widgetDrafts[slot].bus,normalized=input.value.trim().toUpperCase(),item=catalogRouteMatch(catalogState[slot].busRoutes.items,normalized);if(!normalized||(!item&&!isRouteCode(normalized))){if(bus.route_id){requestVersion[slot]++;bus.route_id='';bus.route_label_tc='';clearBusAfterRoute(slot);renderWidgetCards()}markRouteSearchInput(input,Boolean(normalized));return}const route=item?.id||normalized;markRouteSearchInput(input,false,item?'':'本機目錄沒有此路線，可到「更新及省電」更新所有路線。');input.value=route;if(bus.route_id===route)return;requestVersion[slot]++;bus.route_id=route;bus.route_label_tc=item?.label_tc||route;clearBusAfterRoute(slot);renderWidgetCards();ensureCatalogForSlot(slot)}
function setBusDirection(slot,select){requestVersion[slot]++;const bus=widgetDrafts[slot].bus;const option=select.selectedOptions[0];bus.direction_id=option?.dataset.direction||'';bus.service_type=option?.dataset.service||'';bus.destination_label_tc=option?.dataset.destination||option?.textContent||'';bus.stop_id='';bus.stop_label_tc='';resetCatalog(slot,'busStops');renderWidgetCards();ensureCatalogForSlot(slot)}
function setBusStop(slot,select){const bus=widgetDrafts[slot].bus;bus.stop_id=select.value;bus.stop_label_tc=select.selectedOptions[0]?.textContent||'';renderWidgetCards()}
function markGmbRouteSearchInput(input,invalid,message){markRouteSearchInput(input,invalid,message)}
function setGmbRouteSearch(slot,input){const gmb=widgetDrafts[slot].gmb,normalized=input.value.trim().toUpperCase(),item=catalogRouteMatch(catalogState[slot].gmbRoutes.items,normalized);if(!normalized||(!item&&!isRouteCode(normalized))){if(gmb.route_code){requestVersion[slot]++;gmb.route_code='';gmb.route_label_tc='';clearGmbAfterRoute(slot);renderWidgetCards()}markGmbRouteSearchInput(input,Boolean(normalized));return}const route=item?.id||normalized;markGmbRouteSearchInput(input,false,item?'':'本機目錄沒有此路線，可到「更新及省電」更新所有路線。');input.value=route;if(gmb.route_code===route)return;requestVersion[slot]++;gmb.route_code=route;gmb.route_label_tc=item?.label_tc||route;clearGmbAfterRoute(slot);widgetErrors[slot]='';renderWidgetCards();ensureCatalogForSlot(slot)}
function setGmbDirection(slot,select){requestVersion[slot]++;const gmb=widgetDrafts[slot].gmb,option=select.selectedOptions[0];gmb.region=option?.dataset.region||'';gmb.route_id=option?.dataset.routeId||'';gmb.route_seq=option?.dataset.routeSeq||'';gmb.direction_label_tc=option?.textContent?.trim()||'';gmb.stop_id='';gmb.stop_seq='';gmb.stop_label_tc='';resetCatalog(slot,'gmbStops');renderWidgetCards();ensureCatalogForSlot(slot)}
function setGmbStop(slot,select){const gmb=widgetDrafts[slot].gmb,option=select.selectedOptions[0];gmb.stop_seq=select.value;gmb.stop_id=option?.dataset.stopId||'';gmb.stop_label_tc=option?.textContent?.trim()||'';renderWidgetCards()}
function setRailMode(slot,value){requestVersion[slot]++;widgetDrafts[slot].mtr.mode=value;clearRailAfterMode(slot);renderWidgetCards();ensureCatalogForSlot(slot)}
function setRailLine(slot,select){requestVersion[slot]++;const mtr=widgetDrafts[slot].mtr;mtr.line_or_route_id=select.value;mtr.line_or_route_label_tc=select.selectedOptions[0]?.textContent||'';mtr.station_id='';mtr.station_label_tc='';mtr.direction_id='';mtr.direction_label_tc='';resetCatalog(slot,'railStations','railDirections');renderWidgetCards();ensureCatalogForSlot(slot)}
function setRailStation(slot,select){requestVersion[slot]++;const mtr=widgetDrafts[slot].mtr;mtr.station_id=select.value;mtr.station_label_tc=select.selectedOptions[0]?.textContent||'';mtr.direction_id='';mtr.direction_label_tc='';resetCatalog(slot,'railDirections');renderWidgetCards();ensureCatalogForSlot(slot)}
function setRailDirection(slot,select){const mtr=widgetDrafts[slot].mtr;mtr.direction_id=select.value;mtr.direction_label_tc=select.selectedOptions[0]?.textContent||'';renderWidgetCards()}
function setJourneyLocation(slot,select){requestVersion[slot]++;const journey=widgetDrafts[slot].journey_time;journey.location_id=select.value;journey.location_label_tc=select.selectedOptions[0]?.textContent?.trim()||'';clearJourneyAfterLocation(slot);widgetErrors[slot]='';renderWidgetCards();ensureCatalogForSlot(slot)}
function setJourneyDestination(slot,select){const journey=widgetDrafts[slot].journey_time;journey.destination_id=select.value;journey.destination_label_tc=select.selectedOptions[0]?.textContent?.trim()||'';widgetErrors[slot]='';renderWidgetCards()}
async function loadCatalog(slot,key,path){
  const entry=catalogState[slot][key];if(entry.status==='loading'||entry.status==='loaded')return;entry.status='loading';entry.error='';
  renderWidgetCards();
  const token=++requestVersion[slot];
  try{const response=await api(path);if(token!==requestVersion[slot])return;entry.items=Array.isArray(response.data)?response.data:[];entry.status='loaded';renderWidgetCards();ensureCatalogForSlot(slot)}catch(error){if(token!==requestVersion[slot])return;entry.status='error';entry.error=error.message;widgetErrors[slot]=`目錄載入失敗：${error.message}`;renderWidgetCards()}
}
async function loadStopsForSlot(slot,key,provider){
  const entry=catalogState[slot][key];if(entry.status==='loading'||entry.status==='loaded')return;entry.status='loading';entry.error='';renderWidgetCards();const token=++requestVersion[slot];
  try{await loadProviderCatalog(provider);if(token!==requestVersion[slot])return;entry.status='idle';renderWidgetCards();ensureCatalogForSlot(slot)}catch(error){if(token!==requestVersion[slot])return;entry.status='error';entry.error=error.message;widgetErrors[slot]=`本地站牌目錄載入失敗：${error.message}`;renderWidgetCards()}
}
function addRouteItem(entry,route){if(!route||entry.status!=='loaded'||entry.items.some(item=>item.id===route))return;entry.items=[...entry.items,{id:route,label_tc:route}].sort((a,b)=>a.id.localeCompare(b.id,undefined,{numeric:true}))}
function applyRouteOverride(slot,override,strict=false){const widget=widgetDrafts[slot],catalog=catalogState[slot];if(override.kind==='bus'&&widget.type==='bus_eta'&&widget.bus.route_id===override.route&&widget.bus.operator===override.operator){addRouteItem(catalog.busRoutes,override.route);catalog.busDirections=loadedEntry(override.directions||[]);const key=busDirectionValue(widget.bus),stops=override.stops?.[key];if(key&&Array.isArray(stops))catalog.busStops=loadedEntry(stops);else if(strict&&key){widget.bus.direction_id='';widget.bus.service_type='';widget.bus.destination_label_tc='';widget.bus.stop_id='';widget.bus.stop_label_tc='';catalog.busStops=emptyCatalogEntry()}}else if(override.kind==='gmb'&&widget.type==='gmb_eta'&&widget.gmb.route_code===override.route){addRouteItem(catalog.gmbRoutes,override.route);catalog.gmbDirections=loadedEntry(override.directions||[]);const key=gmbDirectionValue(widget.gmb),stops=override.stops?.[key];if(key&&Array.isArray(stops))catalog.gmbStops=loadedEntry(stops);else if(strict&&key){widget.gmb.region='';widget.gmb.route_id='';widget.gmb.route_seq='';widget.gmb.direction_label_tc='';widget.gmb.stop_id='';widget.gmb.stop_seq='';widget.gmb.stop_label_tc='';catalog.gmbStops=emptyCatalogEntry()}}}
function updatedRouteExists(kind,operator,route){const items=kind==='bus'?updatedBusRoutes(operator):localCatalog.routeIndex?.gmb;return Boolean((items||[]).some(item=>String(item.id).toUpperCase()===String(route).toUpperCase()))}
async function requestRouteRefresh(slot,kind,refreshSharedStops=true,force=false){const widget=widgetDrafts[slot],route=kind==='bus'?widget.bus.route_id:widget.gmb.route_code,operator=kind==='bus'?widget.bus.operator:'',key=overrideKey(kind,operator,route),promiseKey=`refresh:${key}`;if(!route)return null;if(!force&&localCatalog.overrides.has(key)){applyRouteOverride(slot,localCatalog.overrides.get(key));return localCatalog.overrides.get(key)}if(localCatalog.overridePromises.has(promiseKey))return localCatalog.overridePromises.get(promiseKey);const promise=portalFetch('/api/catalog/route-refresh',{method:'POST',headers:{'Content-Type':'application/json',[csrfHeader]:csrfToken},body:JSON.stringify({kind,operator,route,refresh_routes:false,refresh_shared_stops:refreshSharedStops})}).then(async response=>{const body=await response.text();if(!response.ok)throw new Error(body);return JSON.parse(body)}).then(updated=>{localCatalog.overrides.set(key,updated);applyRouteOverride(slot,updated,true);return updated}).finally(()=>localCatalog.overridePromises.delete(promiseKey));localCatalog.overridePromises.set(promiseKey,promise);return promise}
async function loadRouteOverrideForSlot(slot){const widget=widgetDrafts[slot],kind=widget.type==='bus_eta'?'bus':widget.type==='gmb_eta'?'gmb':'',route=kind==='bus'?widget.bus.route_id:kind==='gmb'?widget.gmb.route_code:'',operator=kind==='bus'?widget.bus.operator:'';if(!kind||!route)return;const key=overrideKey(kind,operator,route);if(localCatalog.overrides.has(key)){applyRouteOverride(slot,localCatalog.overrides.get(key));return}if(localCatalog.overridePromises.has(key))return;const query=`kind=${kind}&route=${encodeURIComponent(route)}${kind==='bus'?`&operator=${encodeURIComponent(operator)}`:''}`;const promise=portalFetch(`/api/catalog/route-override?${query}`).then(async response=>{if(response.status===404)return null;if(!response.ok)throw new Error(await response.text());return response.json()}).then(async result=>{if(result){localCatalog.overrides.set(key,result);applyRouteOverride(slot,result);return result}if(localCatalog.routeIndex?.updated_at&&updatedRouteExists(kind,operator,route))return requestRouteRefresh(slot,kind,true);return null}).catch(error=>{widgetErrors[slot]=`未能準備最新站牌：${error.message}`;return null}).finally(()=>localCatalog.overridePromises.delete(key));localCatalog.overridePromises.set(key,promise);await promise;if(slot===expandedSlot)renderWidgetCards()}
function ensureCatalogForSlot(slot){
  if(slot!==expandedSlot||!localCatalog.index||!localCatalog.rail)return;const widget=widgetDrafts[slot],catalog=catalogState[slot];let changed=false;
  if(widget.type==='bus_eta'){
    const provider=providerKey(widget.bus.operator);
    if(catalog.busRoutes.status==='idle'){catalog.busRoutes=loadedEntry(busRouteItems(widget.bus.operator));changed=true}
    if(widget.bus.route_id&&catalog.busDirections.status==='idle'){catalog.busDirections=loadedEntry(busDirectionItems(widget.bus.operator,widget.bus.route_id));changed=true;loadRouteOverrideForSlot(slot)}
    if(widget.bus.direction_id&&catalog.busStops.status==='idle'){
      const override=routeOverride('bus',widget.bus.operator,widget.bus.route_id),overrideStops=override?.stops?.[busDirectionValue(widget.bus)];
      if(Array.isArray(overrideStops)){catalog.busStops=loadedEntry(overrideStops);changed=true}
      else{if(!localCatalog.providers[provider])return loadStopsForSlot(slot,'busStops',provider);const selected=busDirectionItems(widget.bus.operator,widget.bus.route_id).find(item=>item.direction_id===widget.bus.direction_id&&item.service_type===widget.bus.service_type);catalog.busStops=loadedEntry(localCatalog.providers[provider].routes?.[selected?.stop_key]||[]);changed=true}
    }
  }else if(widget.type==='gmb_eta'){
    if(catalog.gmbRoutes.status==='idle'){catalog.gmbRoutes=loadedEntry(gmbRouteItems());changed=true}
    if(widget.gmb.route_code&&catalog.gmbDirections.status==='idle'){catalog.gmbDirections=loadedEntry(gmbDirectionItems(widget.gmb.route_code));changed=true;loadRouteOverrideForSlot(slot)}
    if(widget.gmb.route_id&&widget.gmb.route_seq&&catalog.gmbStops.status==='idle'){
      const override=routeOverride('gmb','',widget.gmb.route_code),overrideStops=override?.stops?.[gmbDirectionValue(widget.gmb)];
      if(Array.isArray(overrideStops)){catalog.gmbStops=loadedEntry(overrideStops);changed=true}
      else{if(!localCatalog.providers.gmb)return loadStopsForSlot(slot,'gmbStops','gmb');catalog.gmbStops=loadedEntry(localCatalog.providers.gmb.routes?.[gmbDirectionValue(widget.gmb)]||[]);changed=true}
    }
  }else if(widget.type==='mtr_eta'){
    const group=activeRailGroup(widget);
    if(catalog.railLines.status==='idle'){catalog.railLines=loadedEntry(railGroups(widget.mtr.mode).map(item=>({id:item.id,label_tc:item.label_tc})));changed=true}
    if(widget.mtr.line_or_route_id&&catalog.railStations.status==='idle'){catalog.railStations=loadedEntry(group?.stations||[]);changed=true}
    if(widget.mtr.station_id&&catalog.railDirections.status==='idle'){catalog.railDirections=loadedEntry(group?.directions||[]);changed=true}
  }else if(widget.type==='journey_time'){
    if(catalog.journeyLocations.status==='idle')return loadCatalog(slot,'journeyLocations','/api/catalog/journey/locations');
    if(widget.journey_time.location_id&&catalog.journeyDestinations.status==='idle')return loadCatalog(slot,'journeyDestinations',`/api/catalog/journey/destinations?location=${encodeURIComponent(widget.journey_time.location_id)}`);
  }
  if(changed)renderWidgetCards();
}
function setCatalogUpdateUi(message){const button=byId('catalog_update_button');if(button){const loading=catalogUpdateState==='loading';button.disabled=loading;button.setAttribute('aria-busy',String(loading));button.textContent=loading?'正在更新所有路線':'找不到站牌？更新所有路線'}byId('catalog_update_status').textContent=message}
async function refreshAllRoutes(){if(catalogUpdateState==='loading')return;catalogUpdateState='loading';setCatalogUpdateUi('裝置正在取得九巴、龍運、城巴及專線小巴路線索引…');try{const response=await portalFetch('/api/catalog/update',{method:'POST',headers:{'Content-Type':'application/json',[csrfHeader]:csrfToken},body:'{}'}),body=await response.text();if(!response.ok)throw new Error(body);const updatedIndex=JSON.parse(body);if(!updatedIndex?.bus?.kmb||!updatedIndex?.bus?.ctb||!updatedIndex?.gmb)throw new Error('更新路線索引格式不正確');localCatalog.routeIndex=updatedIndex;catalogState.forEach((catalog,slot)=>{const widget=widgetDrafts[slot];if(widget.type==='bus_eta')catalog.busRoutes=loadedEntry(busRouteItems(widget.bus.operator));if(widget.type==='gmb_eta')catalog.gmbRoutes=loadedEntry(gmbRouteItems())});const targets=[],seen=new Set();widgetDrafts.forEach((widget,slot)=>{const kind=widget.type==='bus_eta'?'bus':widget.type==='gmb_eta'?'gmb':'',route=kind==='bus'?widget.bus.route_id:kind==='gmb'?widget.gmb.route_code:'',operator=kind==='bus'?widget.bus.operator:'';if(!kind||!route)return;const key=overrideKey(kind,operator,route);if(!seen.has(key)){seen.add(key);targets.push({slot,kind,route,operator})}});let firstKmb=true,failures=[];for(let index=0;index<targets.length;index++){const target=targets[index];setCatalogUpdateUi(`已更新所有路線索引；正在更新小工具站牌 ${index+1}/${targets.length}：${target.route}`);try{const refreshShared=target.kind!=='bus'||target.operator==='ctb'||firstKmb;await requestRouteRefresh(target.slot,target.kind,refreshShared,true);if(target.kind==='bus'&&target.operator!=='ctb')firstKmb=false}catch(error){failures.push(`${target.route}：${error.message}`)}}if(savedConfig?.catalog){savedConfig.catalog.last_checked_at=updatedIndex.updated_at||0}catalogUpdateState='idle';renderWidgetCards();ensureCatalogForSlot(expandedSlot);renderCatalogFacts(savedConfig?.catalog||{});setCatalogUpdateUi(failures.length?`所有路線索引已更新；${failures.length} 個小工具站牌未能更新：${failures.join('；')}`:`所有路線已更新${targets.length?`，並已保存 ${targets.length} 個小工具使用的最新站牌`:''}。`)}catch(error){catalogUpdateState='idle';setCatalogUpdateUi(`未能更新所有路線：${error.message}`)}}
function isCatalogLoadingForSlot(slot){return catalogKeys.some(key=>catalogState[slot][key].status==='loading')}
function validateWidgetDrafts(){
  let valid=true;firstWidgetValidation=null;widgetDrafts.forEach((widget,slot)=>{let message='',fieldId='';if(widget.type==='bus_eta'){if(!widget.bus.operator)fieldId=`bus_operator_${slot}`;else if(!widget.bus.route_id)fieldId=`bus_route_${slot}`;else if(!(widget.bus.direction_id&&widget.bus.service_type))fieldId=`bus_direction_${slot}`;else if(!widget.bus.stop_id)fieldId=`bus_stop_${slot}`;if(fieldId)message='請完成營辦商、路線、方向及站牌設定。'}if(widget.type==='gmb_eta'){if(!widget.gmb.route_code)fieldId=`gmb_route_${slot}`;else if(!(widget.gmb.region&&widget.gmb.route_id&&widget.gmb.route_seq))fieldId=`gmb_direction_${slot}`;else if(!(widget.gmb.stop_id&&widget.gmb.stop_seq))fieldId=`gmb_stop_${slot}`;if(fieldId)message='請完成路線、方向及班次、站點設定。'}if(widget.type==='mtr_eta'){if(!widget.mtr.mode)fieldId=`rail_mode_${slot}`;else if(!widget.mtr.line_or_route_id)fieldId=`rail_line_${slot}`;else if(!widget.mtr.station_id)fieldId=`rail_station_${slot}`;else if(!widget.mtr.direction_id)fieldId=`rail_direction_${slot}`;if(fieldId)message='請完成鐵路類型、路線、車站及方向設定。'}if(widget.type==='journey_time'){if(!widget.journey_time.location_id)fieldId=`journey_location_${slot}`;else if(!widget.journey_time.destination_id)fieldId=`journey_destination_${slot}`;if(fieldId)message='請完成指示地點及行車方向設定。'}widgetErrors[slot]=message;if(message){valid=false;if(!firstWidgetValidation)firstWidgetValidation={slot,fieldId}}});renderWidgetCards();return valid;
}
function collectConfig(){return{schema_version:2,wifi_ssid:byId('wifi_ssid').value.trim(),wifi_password:byId('wifi_password').value,weather_location_tc:byId('weather_location').value||'香港天文台',sleep_enabled:byId('sleep_enabled').checked,wake_duration_minutes:Number(byId('wake_duration_minutes').value||5),sleep_maintenance_hours:Number(byId('sleep_maintenance_hours').value||0),widgets:widgetDrafts.map(widget=>{const out={type:widget.type};if(widget.type==='bus_eta')out.bus={...widget.bus};if(widget.type==='gmb_eta')out.gmb={...widget.gmb};if(widget.type==='mtr_eta')out.mtr={...widget.mtr};if(widget.type==='journey_time')out.journey_time={...widget.journey_time};return out})}}
function renderWeatherLocationOptions(selected){const select=byId('weather_location');select.innerHTML=weatherLocations.map(value=>`<option ${value===selected?'selected':''}>${escapeHtml(value)}</option>`).join('');if(selected&&!weatherLocations.includes(selected))select.insertAdjacentHTML('beforeend',`<option selected>${escapeHtml(selected)}</option>`);select.value=selected||'香港天文台'}
function renderDeviceFacts(cfg){byId('firmware_version').textContent=cfg.firmware_version||'未能讀取';const battery=cfg.battery||{};byId('battery_percent').textContent=battery.valid?`${battery.percent}%`:'未能讀取';byId('battery_state').textContent=!battery.valid?'未能讀取':battery.full?'已充滿':battery.charging?'充電中':battery.power_present?'外接電源':'電池供電';byId('connection_summary').textContent=`${cfg.wifi_ssid||'未連線'}　${battery.valid?`${battery.percent}%`:'電量未明'}`}
function renderCatalogFacts(catalog){byId('catalog_revision').textContent=catalog?.revision||'未能讀取';byId('catalog_source').textContent='韌體內建';byId('catalog_size').textContent=Number.isFinite(Number(catalog?.bytes))?`${(Number(catalog.bytes)/1024).toFixed(0)} KB`:'未能讀取';const checked=Number(catalog?.last_checked_at||0);byId('catalog_last_checked').textContent=checked>0?new Date(checked*1000).toLocaleString('zh-HK'):'尚未更新所有路線';if(catalogUpdateState!=='loading')setCatalogUpdateUi(catalog?.update_available===false?'LittleFS 未能使用，暫時只提供內建目錄。':'更新資料會保存於裝置 LittleFS。')}
async function scanWifi(){byId('wifi_scan_status').textContent='正在掃描';try{const result=await api('/api/wifi');const select=byId('wifi_network');select.innerHTML='<option value="">請選擇網絡</option>'+optionRows(result.data||[],'');byId('wifi_scan_status').textContent=(result.data||[]).length?`找到 ${(result.data||[]).length} 個網絡`:'找不到網絡，可手動輸入'}catch(error){byId('wifi_scan_status').textContent=`掃描失敗：${error.message}`}}
function selectWifiNetwork(value){if(value)byId('wifi_ssid').value=value}
function applyConfig(cfg){savedConfig=cfg;csrfToken=cfg.csrf_token||'';byId('current_ssid').textContent=cfg.wifi_ssid||'尚未設定';byId('password_status').textContent=cfg.wifi_password_set?'已儲存密碼':'尚未儲存密碼';byId('wifi_ssid').value=cfg.wifi_ssid||'';byId('wifi_password').value='';renderWeatherLocationOptions(cfg.weather_location_tc||'香港天文台');byId('sleep_enabled').checked=cfg.sleep_enabled!==false;byId('wake_duration_minutes').value=cfg.wake_duration_minutes||5;byId('sleep_maintenance_hours').value=cfg.sleep_maintenance_hours??12;widgetDrafts=Array.from({length:4},(_,slot)=>cfg.widgets?.[slot]?{...emptyWidget(),...cfg.widgets[slot],bus:{...emptyWidget().bus,...(cfg.widgets[slot].bus||{})},gmb:{...emptyWidget().gmb,...(cfg.widgets[slot].gmb||{})},mtr:{...emptyWidget().mtr,...(cfg.widgets[slot].mtr||{})},journey_time:{...emptyWidget().journey_time,...(cfg.widgets[slot].journey_time||{})}}:emptyWidget());renderDeviceFacts(cfg);renderCatalogFacts(cfg.catalog||{})}
async function loadPortal(){const[cfg]=await Promise.all([api('/api/config',{cache:'no-store'}),loadLocalCatalog(),loadUpdatedRouteIndex()]);applyConfig(cfg);renderWidgetCards();ensureCatalogForSlot(expandedSlot);const required=new Set(widgetDrafts.flatMap(widget=>widget.type==='bus_eta'?[providerKey(widget.bus.operator)]:widget.type==='gmb_eta'?['gmb']:[]));Promise.all([...required].map(provider=>loadProviderCatalog(provider))).then(()=>ensureCatalogForSlot(expandedSlot)).catch(()=>{})}
async function saveConfig(event){event.preventDefault();if(isCatalogLoadingForSlot(expandedSlot)){selectTab('widgets');byId('save_status').textContent='選項仍在載入，請稍候。';return}if(!byId('wifi_ssid').value.trim()){selectTab('wifi');byId('save_status').textContent='請輸入 Wi-Fi 名稱。';byId('wifi_ssid').focus();return}if(!validateWidgetDrafts()){expandedSlot=firstWidgetValidation.slot;renderWidgetCards();selectTab('widgets');byId('save_status').textContent='請完成小工具設定。';setTimeout(()=>byId(firstWidgetValidation.fieldId)?.focus(),0);return}const payload=collectConfig();const button=event.submitter||document.querySelector('.primary');button.disabled=true;byId('save_status').textContent='正在驗證及儲存設定';try{const response=await portalFetch('/api/save',{method:'POST',headers:{'Content-Type':'application/json',[csrfHeader]:csrfToken},body:JSON.stringify(payload)});const message=await response.text();if(!response.ok)throw new Error(message);byId('save_status').textContent=message}catch(error){byId('save_status').textContent=`未能儲存：${error.message}`;button.disabled=false}}
byId('config_form').addEventListener('submit',saveConfig);
loadPortal().catch(error=>{byId('save_status').textContent=`未能載入設定或本地交通目錄：${error.message}`});
</script>
</body>
</html>
)HTML";
