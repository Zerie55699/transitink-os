import json
import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class PortalJavaScriptBehaviorTests(unittest.TestCase):
    def test_widget_pages_render_four_positions_and_move_across_page_boundaries(self):
        source = (ROOT / "src/TransitInkPortalPage.cpp").read_text()
        script = source.split("<script>", 1)[1].split("</script>", 1)[0]
        script = script.split("byId('config_form').addEventListener", 1)[0]
        harness = r"""
const elements={};
function element(id){return elements[id]||(elements[id]={id,value:'',checked:false,hidden:false,innerHTML:'',textContent:'',disabled:false,selectedOptions:[],setAttribute(name,value){this[name]=value},insertAdjacentHTML(){},focus(){}})}
global.document={getElementById:element,querySelector(){return element('primary')},querySelectorAll(){return[]}};
element('wifi_ssid').value='TransitInk';
element('weather_location').value='香港天文台';
widgetDrafts=Array.from({length:widgetSlotCount},()=>emptyWidget());
widgetDrafts[3].type='bus_eta';
widgetDrafts[3].bus.route_id='43X';
widgetDrafts[4].type='journey_time';
widgetDrafts[4].journey_time.location_id='H1';
widgetDrafts[4].journey_time.destination_id='CH';
renderWidgetCards();
if((element('widget_page_tabs').innerHTML.match(/widget-page-tab/g)||[]).length!==3)throw new Error('沒有三個頁面選擇');
if((element('widget_cards').innerHTML.match(/widget-card/g)||[]).length!==4)throw new Error('頁面沒有維持四個位置');
if(!element('widget_page_tabs').innerHTML.includes('已設定 1/4'))throw new Error('頁面沒有顯示已設定數量');
selectWidgetPage(1);
if(selectedWidgetPage!==1||expandedSlot!==4)throw new Error('切換頁面沒有同步展開位置');
if(!element('widget_cards').innerHTML.includes('第 2 頁，位置 1'))throw new Error('位置沒有標示所屬頁面');
moveWidget(4,-1);
if(selectedWidgetPage!==0||expandedSlot!==3||widgetDrafts[3].type!=='journey_time'||widgetDrafts[4].type!=='bus_eta')throw new Error('跨頁移動沒有保持全域次序');
const payload=collectConfig();
if(payload.schema_version!==3||payload.widgets.length!==12)throw new Error('沒有輸出三頁設定');
process.stdout.write('ok');
"""
        completed = subprocess.run(
            ["node", "-e", script + harness],
            cwd=ROOT,
            text=True,
            capture_output=True,
            timeout=10,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(completed.stdout, "ok")

    def test_first_setup_uses_embedded_london_bus_and_rail_catalogues(self):
        source = (ROOT / "src/TransitInkPortalPage.cpp").read_text()
        script = source.split("<script>", 1)[1].split("</script>", 1)[0]
        script = script.split("byId('config_form').addEventListener", 1)[0]
        harness = r"""
const elements={};
function element(id){return elements[id]||(elements[id]={id,value:'',checked:false,hidden:false,innerHTML:'',textContent:'',disabled:false,selectedOptions:[],dataset:{},setAttribute(name,value){this[name]=value},setCustomValidity(){},insertAdjacentHTML(){},focus(){}})}
global.document={getElementById:element,querySelector(){return element('primary')},querySelectorAll(){return[]}};
global.fetch=path=>Promise.reject(new Error(`unexpected online catalogue request: ${path}`));
localCatalog.index={bus:{tfl:{routes:{'24':[{
  direction_id:'inbound',service_type:'A|B',
  origin_label_tc:'South End Green',destination_label_tc:'Grosvenor Road',
  origin_label_en:'South End Green',destination_label_en:'Grosvenor Road',
  stop_key:'24:inbound:A|B'
}]}}}};
localCatalog.providers.tfl={routes:{'24:inbound:A|B':[
  {id:'490012280A',label_tc:'South End Green',label_en:'South End Green',sequence:1}
]}};
localCatalog.rail={modes:{london_rail:[{
  id:'victoria',label_tc:'Victoria',label_en:'Victoria',
  stations:[{id:'940GZZLUOXC',label_tc:'Oxford Circus',label_en:'Oxford Circus'}],
  directions:[{id:'inbound',label_tc:'Inbound',label_en:'Inbound'}]
}]}};

widgetDrafts[0]=emptyWidget();
widgetDrafts[0].type='bus_eta';
widgetDrafts[0].bus.operator='tfl';
widgetDrafts[0].bus.route_id='24';
expandedSlot=0;
ensureCatalogForSlot(0);
if(catalogState[0].busRoutes.items[0]?.id!=='24')throw new Error('Embedded London bus route was not listed');
if(catalogState[0].busDirections.items[0]?.service_type!=='A|B')throw new Error('Embedded London bus direction was not listed');
widgetDrafts[0].bus.direction_id='inbound';
widgetDrafts[0].bus.service_type='A|B';
ensureCatalogForSlot(0);
if(catalogState[0].busStops.items[0]?.id!=='490012280A')throw new Error('Embedded London bus stop was not listed');

  widgetDrafts[0]=emptyWidget();
  widgetDrafts[0].type='mtr_eta';
  widgetDrafts[0].mtr.mode='london_rail';
  widgetDrafts[0].mtr.line_or_route_id='victoria';
  widgetDrafts[0].mtr.station_id='940GZZLUOXC';
  expandedSlot=0;
  catalogState[0]=emptyCatalogState();
  ensureCatalogForSlot(0);
  if(catalogState[0].railLines.items[0]?.id!=='victoria')throw new Error('Embedded London rail line was not listed');
  if(catalogState[0].railStations.items[0]?.id!=='940GZZLUOXC')throw new Error('Embedded London rail station was not listed');
  if(catalogState[0].railDirections.items[0]?.id!=='inbound')throw new Error('Embedded London rail direction was not listed');
  process.stdout.write('ok');
"""
        completed = subprocess.run(
            ["node", "-e", "global.location={pathname:'/'};" + script + harness],
            cwd=ROOT,
            text=True,
            capture_output=True,
            timeout=10,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(completed.stdout, "ok")

    def test_language_switch_rerenders_widgets_and_persists_locale(self):
        source = (ROOT / "src/TransitInkPortalPage.cpp").read_text()
        script = source.split("<script>", 1)[1].split("</script>", 1)[0]
        script = script.split("byId('config_form').addEventListener", 1)[0]
        harness = r"""
const elements={};
function element(id){return elements[id]||(elements[id]={id,value:'',checked:false,hidden:false,innerHTML:'',textContent:'',disabled:false,selectedOptions:[],setAttribute(name,value){this[name]=value},insertAdjacentHTML(){},focus(){}})}
global.document={
  title:'',
  documentElement:{setAttribute(name,value){this[name]=value}},
  getElementById:element,
  querySelector(){return element('primary')},
  querySelectorAll(){return[]}
};
element('wifi_ssid').value='TransitInk';
element('weather_location').value='uk:london';
element('time_zone').value='Europe/London';
element('display_font').value='unifont';
widgetDrafts=Array.from({length:widgetSlotCount},()=>emptyWidget());
widgetDrafts[0].type='bus_eta';
expandedSlot=0;
setPortalLocale('en-GB');
const english=element('widget_cards').innerHTML;
if(portalLocale!=='en-GB'||element('ui_locale').value!=='en-GB')throw new Error('English locale was not applied');
if(document.documentElement.lang!=='en-GB'||!document.title.includes('Device settings'))throw new Error('Document language was not updated');
if(!english.includes('Widget type')||!english.includes('Move up')||!english.includes('Bus setup is incomplete'))throw new Error('Dynamic widget text was not translated');
setWeatherRegion('uk');
if(element('weather_region').value!=='uk'||element('weather_location').value!=='uk:london'||element('weather_location').innerHTML.includes('Hong Kong Observatory')||element('uk_weather_attribution').hidden)throw new Error('UK weather locations were not filtered at the parent level');
setWeatherRegion('hk');
if(element('weather_region').value!=='hk'||element('weather_location').value!=='香港天文台'||element('weather_location').innerHTML.includes('London')||!element('uk_weather_attribution').hidden)throw new Error('Hong Kong weather locations were not isolated');
setWeatherRegion('uk');
if(collectConfig().ui_locale!=='en-GB'||collectConfig().display_font!=='unifont'||collectConfig().weather_location_tc!=='uk:london'||collectConfig().time_zone!=='Europe/London')throw new Error('English locale, display font, UK weather, or time zone was not included in the saved config');
setPortalLocale('zh-HK');
if(!element('widget_cards').innerHTML.includes('小工具類型')||collectConfig().ui_locale!=='zh-HK')throw new Error('Traditional Chinese locale was not restored');
process.stdout.write('ok');
"""
        completed = subprocess.run(
            ["node", "-e", script + harness],
            cwd=ROOT,
            text=True,
            capture_output=True,
            timeout=10,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(completed.stdout, "ok")

    def test_lan_portal_fetches_include_session_access_token(self):
        source = (ROOT / "src/TransitInkPortalPage.cpp").read_text()
        script = source.split("<script>", 1)[1].split("</script>", 1)[0]
        script = script.split("byId('config_form').addEventListener", 1)[0]
        harness = r"""
let captured=null;
global.fetch=(path,options)=>{captured={path,options};return Promise.resolve({ok:true,json:async()=>({})})};
(async()=>{
  await portalFetch('/api/config',{headers:{Accept:'application/json'}});
  if(captured?.path!=='/api/config')throw new Error('要求路徑不正確');
  if(captured?.options?.headers?.['X-TransitInk-Access']!=='SESSION123')throw new Error('沒有附加 LAN session token');
  if(captured?.options?.headers?.Accept!=='application/json')throw new Error('覆蓋了原有 headers');
  process.stdout.write('ok');
})().catch(error=>{console.error(error);process.exitCode=1});
"""
        completed = subprocess.run(
            ["node", "-e", "global.location={pathname:'/SESSION123'};" + script + harness],
            cwd=ROOT,
            text=True,
            capture_output=True,
            timeout=10,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(completed.stdout, "ok")

    def test_portal_serializes_device_requests_and_keeps_parallel_catalog_results(self):
        source = (ROOT / "src/TransitInkPortalPage.cpp").read_text()
        script = source.split("<script>", 1)[1].split("</script>", 1)[0]
        script = script.split("byId('config_form').addEventListener", 1)[0]
        harness = r"""
const elements={};
function element(id){return elements[id]||(elements[id]={id,value:'',checked:false,hidden:false,innerHTML:'',textContent:'',disabled:false,selectedOptions:[],setAttribute(){},insertAdjacentHTML(){},focus(){}})}
global.document={getElementById:element,querySelector(){return element('primary')}};
const calls=[];
let releaseFirst;
global.fetch=(path)=>{
  calls.push(path);
  if(path==='/first')return Promise.resolve({ok:true,status:200,statusText:'OK',headers:{},arrayBuffer:()=>new Promise(resolve=>{releaseFirst=()=>resolve(new ArrayBuffer(0))})});
  if(path==='/second')return Promise.resolve({ok:true});
  if(path==='/locations')return Promise.resolve({ok:true,json:async()=>({data:[{id:'H1',label_tc:'告士打道'}]})});
  if(path==='/destinations')return Promise.resolve({ok:true,json:async()=>({data:[{id:'CH',label_tc:'紅磡海底隧道'}]})});
  throw new Error(`未預期要求：${path}`);
};
(async()=>{
  const first=portalFetch('/first');
  const second=portalFetch('/second');
  await Promise.resolve();
  await Promise.resolve();
  if(calls.join('|')!=='/first')throw new Error(`裝置收到並行要求：${calls.join('|')}`);
  releaseFirst();
  await first;
  await second;
  if(calls.join('|')!=='/first|/second')throw new Error(`要求排程不正確：${calls.join('|')}`);

  expandedSlot=3;
  const locations=loadCatalog(0,'journeyLocations','/locations');
  const destinations=loadCatalog(0,'journeyDestinations','/destinations');
  await Promise.all([locations,destinations]);
  if(catalogState[0].journeyLocations.status!=='loaded'||catalogState[0].journeyDestinations.status!=='loaded')throw new Error('同一小工具的目錄要求互相作廢');
  process.stdout.write('ok');
})().catch(error=>{console.error(error.message);process.exit(1)});
"""
        completed = subprocess.run(
            ["node", "-e", script + harness],
            cwd=ROOT,
            text=True,
            capture_output=True,
            timeout=10,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(completed.stdout, "ok")

    def test_local_catalog_labels_are_escaped_before_rendering(self):
        source = (ROOT / "src/TransitInkPortalPage.cpp").read_text()
        script = source.split("<script>", 1)[1].split("</script>", 1)[0]
        script = script.split("byId('config_form').addEventListener", 1)[0]
        harness = r"""
const elements={};
function element(id){return elements[id]||(elements[id]={id,value:'',checked:false,hidden:false,innerHTML:'',textContent:'',disabled:false,selectedOptions:[],setAttribute(){},insertAdjacentHTML(){},focus(){}})}
global.document={getElementById:element,querySelector(){return element('primary')}};
global.fetch=()=>{throw new Error('已載入的裝置快取不應再次連線')};
widgetDrafts[0]=emptyWidget();widgetDrafts[0].type='bus_eta';widgetDrafts[0].bus.route_id='43X';widgetDrafts[0].bus.direction_id='O';widgetDrafts[0].bus.service_type='1';expandedSlot=0;
catalogState[0].busRoutes={status:'loaded',items:[{id:'43X',label_tc:'43X'}],error:''};
catalogState[0].busDirections={status:'loaded',items:[{id:'O:1',label_tc:'甲 往 乙',direction_id:'O',service_type:'1'}],error:''};
catalogState[0].busStops={status:'loaded',items:[{id:'STOP',label_tc:'<img src=x onerror=alert(1)>',sequence:1}],error:''};
renderWidgetCards();
const markup=element('widget_cards').innerHTML;
if(markup.includes('<img src=x'))throw new Error('目錄字串成為可執行 HTML');
if(!markup.includes('&lt;img src=x onerror=alert(1)&gt;'))throw new Error('目錄字串沒有被 escaping');
process.stdout.write('ok');
"""
        completed = subprocess.run(
            ["node", "-e", script + harness],
            cwd=ROOT,
            text=True,
            capture_output=True,
            timeout=10,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(completed.stdout, "ok")

    def test_route_search_accepts_exact_and_missing_codes_for_global_update(self):
        source = (ROOT / "src/TransitInkPortalPage.cpp").read_text()
        self.assertIn('type="search"', source)
        self.assertIn('<datalist id="${id}_options">', source)
        script = source.split("<script>", 1)[1].split("</script>", 1)[0]
        script = script.split("byId('config_form').addEventListener", 1)[0]
        harness = r"""
const elements={};
function element(id){return elements[id]||(elements[id]={id,value:'',checked:false,hidden:false,innerHTML:'',textContent:'',disabled:false,selectedOptions:[],setAttribute(name,value){this[name]=value},setCustomValidity(value){this.validationMessage=value},insertAdjacentHTML(){},focus(){}})}
global.document={getElementById:element,querySelector(){return element('primary')}};
global.fetch=()=>{throw new Error('搜尋測試不應發出網絡要求')};
widgetDrafts=Array.from({length:widgetSlotCount},()=>emptyWidget());
widgetDrafts[0].type='bus_eta';
expandedSlot=1;
catalogState[0].busRoutes={status:'loaded',items:[{id:'43X',label_tc:'43X'},{id:'96',label_tc:'96'}],error:''};
const input={id:'bus_route_0',value:'43x',setAttribute(name,value){this[name]=value},setCustomValidity(value){this.validationMessage=value}};
setBusRouteSearch(0,input);
if(widgetDrafts[0].bus.route_id!=='43X'||widgetDrafts[0].bus.route_label_tc!=='43X')throw new Error('小寫輸入未選中 43X');
if(input.value!=='43X'||input['aria-invalid']!=='false')throw new Error('有效路線未正規化');
widgetDrafts[0].bus.direction_id='O';widgetDrafts[0].bus.service_type='1';widgetDrafts[0].bus.stop_id='STOP';
input.value='999';
setBusRouteSearch(0,input);
if(widgetDrafts[0].bus.route_id!=='999'||widgetDrafts[0].bus.direction_id||widgetDrafts[0].bus.stop_id)throw new Error('缺漏路線沒有保留代號或清除舊站牌');
if(input['aria-invalid']!=='false'||input.validationMessage)throw new Error('可手動更新的路線被當成格式錯誤');
if(!element('bus_route_0_help').textContent.includes('設定'))throw new Error('缺漏路線沒有全路線更新提示');
if(validateWidgetDrafts()||firstWidgetValidation?.fieldId!=='bus_direction_0')throw new Error('缺漏路線沒有要求更新後選方向');
process.stdout.write('ok');
"""
        completed = subprocess.run(
            ["node", "-e", script + harness],
            cwd=ROOT,
            text=True,
            capture_output=True,
            timeout=10,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(completed.stdout, "ok")

    def test_gmb_catalog_chain_and_selected_stop_reach_post(self):
        source = (ROOT / "src/TransitInkPortalPage.cpp").read_text()
        script = source.split("<script>", 1)[1].split("</script>", 1)[0]
        script = script.split("byId('config_form').addEventListener", 1)[0]
        harness = r"""
const elements={};
function element(id){return elements[id]||(elements[id]={id,value:'',checked:false,hidden:false,innerHTML:'',textContent:'',disabled:false,selectedOptions:[],setAttribute(){},insertAdjacentHTML(){},focus(){}})}
global.document={getElementById:element,querySelector(){return element('primary')}};
const calls=[];
let postedConfig=null;
global.fetch=(path,options)=>{
  calls.push(path);
  if(path.startsWith('/api/catalog/route-override'))return Promise.resolve({ok:false,status:404,text:async()=>''});
  if(path==='/api/save'){postedConfig=JSON.parse(options.body);return Promise.resolve({ok:true,text:async()=>"設定已儲存"})}
  throw new Error(`未預期要求：${path}`);
};
(async()=>{
  element('wifi_ssid').value='TransitInk';
  element('weather_location').value='香港天文台';
  widgetDrafts=Array.from({length:widgetSlotCount},()=>emptyWidget());
  localCatalog.index={schema_version:1,revision:'fixture123',bus:{kmb_lwb:{routes:{}},ctb:{routes:{}}},gmb:{routes:{'69':[{id:'2000410:1',label_tc:'數碼港 往 鰂魚涌',region:'HKI',route_id:'2000410',route_seq:'1',origin_label_tc:'數碼港',destination_label_tc:'鰂魚涌',stop_key:'2000410:1'}]}}};
  localCatalog.rail={schema_version:1,revision:'fixture123',modes:{heavy_rail:[],light_rail:[]}};
  localCatalog.providers.gmb={schema_version:1,revision:'fixture123',routes:{'2000410:1':[{id:'1',label_tc:'數碼港公共運輸交匯處',stop_id:'20003337',stop_seq:'1'}]}};
  widgetDrafts[1].type='gmb_eta';
  expandedSlot=1;
  catalogState[1].gmbRoutes={status:'loaded',items:[{id:'69',label_tc:'69'}],error:''};
  renderWidgetCards();
  setGmbRouteSearch(1,{id:'gmb_route_1',value:'69',setAttribute(){},setCustomValidity(){}});
  await new Promise(resolve=>setTimeout(resolve,0));
  setGmbDirection(1,{value:'2000410:1',selectedOptions:[{textContent:'數碼港 往 鰂魚涌',dataset:{region:'HKI',routeId:'2000410',routeSeq:'1'}}]});
  await new Promise(resolve=>setTimeout(resolve,0));
  setGmbStop(1,{value:'1',selectedOptions:[{textContent:'數碼港公共運輸交匯處',dataset:{stopId:'20003337',stopSeq:'1'}}]});
  await saveConfig({preventDefault(){},submitter:element('submit')});
  const saved=postedConfig?.widgets?.[1];
  if(!calls[0]?.startsWith('/api/catalog/route-override?kind=gmb'))throw new Error(`沒有檢查本機路線覆寫：${calls.join('|')}`);
  if(calls.some(path=>path.startsWith('/api/catalog/gmb/')))throw new Error(`本地目錄仍呼叫舊小巴接口：${calls.join('|')}`);
  if(calls[1]!=='/api/save')throw new Error(`儲存要求次序不正確：${calls.join('|')}`);
  if(saved?.type!=='gmb_eta')throw new Error('專線小巴類型沒有送出');
  if(saved.gmb?.route_code!=='69'||saved.gmb?.route_id!=='2000410'||saved.gmb?.route_seq!=='1')throw new Error('專線小巴路線及方向沒有送出');
  if(saved.gmb?.region!=='HKI')throw new Error('專線小巴地區沒有按方向自動保存');
  if(saved.gmb?.stop_id!=='20003337'||saved.gmb?.stop_seq!=='1')throw new Error('專線小巴站點沒有送出');
  if(saved.gmb?.direction_label_tc!=='數碼港 往 鰂魚涌'||saved.gmb?.stop_label_tc!=='數碼港公共運輸交匯處')throw new Error('專線小巴顯示名稱沒有送出');
  process.stdout.write('ok');
})().catch(error=>{console.error(error.message);process.exit(1)});
"""
        completed = subprocess.run(
            ["node", "-e", script + harness],
            cwd=ROOT,
            text=True,
            capture_output=True,
            timeout=10,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(completed.stdout, "ok")

    def test_gmb_route_search_has_no_region_selector_or_region_query(self):
        source = (ROOT / "src/TransitInkPortalPage.cpp").read_text()
        self.assertNotIn("gmb_region_", source)
        self.assertNotIn("setGmbRegion", source)
        self.assertIn("catalog.gmbRoutes", source)
        self.assertIn("searchableRouteField(`gmb_route_${slot}`", source)
        self.assertIn("/assets/catalog/current/index.json", source)
        self.assertIn("/assets/catalog/current/stops-${provider}.json", source)
        self.assertIn("/api/catalog/update", source)
        self.assertIn("/api/catalog/route-refresh", source)
        self.assertNotIn("/api/catalog/gmb/routes?region=", source)

    def test_global_catalog_update_action_lives_only_in_power_panel(self):
        source = (ROOT / "src/TransitInkPortalPage.cpp").read_text()
        power_panel = source.split('id="panel_power"', 1)[1].split("</section>", 1)[0]
        widget_renderer = source.split("function renderWidgetFields", 1)[1].split(
            "function renderWidgetCards", 1
        )[0]
        self.assertIn("找不到站牌？更新所有路線", power_panel)
        self.assertIn('onclick="refreshAllRoutes()"', power_panel)
        self.assertNotIn("更新此路線", source)
        self.assertNotIn("catalog_update_button", widget_renderer)

    def test_firmware_update_is_separate_and_preserves_settings(self):
        source = (ROOT / "src/TransitInkPortalPage.cpp").read_text()
        power_panel = source.split('id="panel_power"', 1)[1].split(
            "</section>", 1
        )[0]
        self.assertIn("更新並保留設定", power_panel)
        self.assertIn("Wi-Fi、小工具及路線設定", power_panel)
        self.assertIn('onclick="checkFirmwareUpdate()"', power_panel)
        self.assertIn('onclick="installFirmwareUpdate()"', power_panel)
        self.assertIn("'/api/firmware/update'", source)
        self.assertIn("[csrfHeader]:csrfToken", source)

    def test_global_catalog_update_refreshes_index_then_configured_stops(self):
        source = (ROOT / "src/TransitInkPortalPage.cpp").read_text()
        script = source.split("<script>", 1)[1].split("</script>", 1)[0]
        script = script.split("byId('config_form').addEventListener", 1)[0]
        harness = r"""
const elements={};
function element(id){return elements[id]||(elements[id]={id,value:'',checked:false,hidden:false,innerHTML:'',textContent:'',disabled:false,selectedOptions:[],setAttribute(name,value){this[name]=value},insertAdjacentHTML(){},focus(){}})}
global.document={getElementById:element,querySelector(){return element('primary')}};
const calls=[];
global.fetch=async(path,options)=>{
  calls.push({path,body:options?.body||''});
  if(path==='/api/catalog/update')return{ok:true,text:async()=>JSON.stringify({updated_at:123,bus:{kmb:[{id:'43X',label_tc:'43X'}],ctb:[]},gmb:[{id:'69',label_tc:'69'}]})};
  if(path==='/api/catalog/route-refresh'){
    const request=JSON.parse(options.body);
    return{ok:true,text:async()=>JSON.stringify(request.kind==='bus'?{kind:'bus',operator:request.operator,route:request.route,directions:[],stops:{},updated_at:123}:{kind:'gmb',route:request.route,directions:[],stops:{},updated_at:123})};
  }
  throw new Error(`未預期要求：${path}`);
};
(async()=>{
  savedConfig={catalog:{revision:'fixture',bytes:1,update_available:true,last_checked_at:0}};
  csrfToken='token';
  localCatalog.index={schema_version:1,revision:'fixture',bus:{kmb_lwb:{routes:{}},ctb:{routes:{}}},gmb:{routes:{}}};
  localCatalog.rail={schema_version:1,revision:'fixture',modes:{heavy_rail:[],light_rail:[]}};
  widgetDrafts=Array.from({length:widgetSlotCount},()=>emptyWidget());
  widgetDrafts[0].type='bus_eta';widgetDrafts[0].bus.operator='kmb';widgetDrafts[0].bus.route_id='43X';
  widgetDrafts[1].type='gmb_eta';widgetDrafts[1].gmb.route_code='69';
  expandedSlot=3;
  await refreshAllRoutes();
  if(calls.map(call=>call.path).join('|')!=='/api/catalog/update|/api/catalog/route-refresh|/api/catalog/route-refresh')throw new Error(`更新次序錯誤：${calls.map(call=>call.path).join('|')}`);
  const busRequest=JSON.parse(calls[1].body),gmbRequest=JSON.parse(calls[2].body);
  if(busRequest.route!=='43X'||busRequest.refresh_routes!==false||busRequest.refresh_shared_stops!==true)throw new Error('巴士站牌沒有沿用已更新索引');
  if(gmbRequest.route!=='69'||gmbRequest.refresh_routes!==false)throw new Error('小巴站牌沒有沿用已更新索引');
  if(localCatalog.routeIndex?.updated_at!==123||localCatalog.overrides.size!==2)throw new Error('更新資料沒有保存到前端狀態');
  if(element('catalog_update_button').textContent!=='找不到站牌？更新所有路線'||element('catalog_update_button').disabled)throw new Error('更新按鈕沒有恢復');
  process.stdout.write('ok');
})().catch(error=>{console.error(error.message);process.exit(1)});
"""
        completed = subprocess.run(
            ["node", "-e", script + harness],
            cwd=ROOT,
            text=True,
            capture_output=True,
            timeout=10,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(completed.stdout, "ok")

    def test_catalog_lifecycle_and_first_validation_focus(self):
        source = (ROOT / "src/TransitInkPortalPage.cpp").read_text()
        script = source.split("<script>", 1)[1].split("</script>", 1)[0]
        script = script.split("byId('config_form').addEventListener", 1)[0]
        harness = r"""
const elements={};
let focused='';
function element(id){return elements[id]||(elements[id]={id,value:'',checked:false,hidden:false,innerHTML:'',textContent:'',disabled:false,selectedOptions:[],setAttribute(){},insertAdjacentHTML(){},focus(){focused=id;}})}
global.document={getElementById:element,querySelector(){return element('primary')}};
const calls=[];
global.fetch=async path=>{
  calls.push(path);
  throw new Error(`未預期要求：${path}`);
};
(async()=>{
  localCatalog.index={schema_version:1,revision:'fixture123',bus:{kmb_lwb:{routes:{'43X':[]}},ctb:{routes:{'1':[]}}},gmb:{routes:{}}};
  localCatalog.rail={schema_version:1,revision:'fixture123',modes:{heavy_rail:[],light_rail:[]}};
  widgetDrafts[0]=emptyWidget();widgetDrafts[0].type='bus_eta';expandedSlot=0;
  ensureCatalogForSlot(0);
  await new Promise(resolve=>setTimeout(resolve,10));
  if(calls.length!==0)throw new Error(`內建九巴索引不應呼叫 API：${calls.join('|')}`);
  if(catalogState[0].busRoutes.status!=='loaded'||catalogState[0].busRoutes.items[0]?.id!=='43X')throw new Error('九巴內建目錄未標記 loaded');
  setBusOperator(0,'ctb');
  await new Promise(resolve=>setTimeout(resolve,10));
  if(calls.length!==0||catalogState[0].busRoutes.items[0]?.id!=='1')throw new Error('parent 變更未讀取城巴內建目錄');

  widgetDrafts=Array.from({length:widgetSlotCount},()=>emptyWidget());
  widgetDrafts[2].type='bus_eta';
  element('wifi_ssid').value='TransitInk';
  await saveConfig({preventDefault(){},submitter:element('submit')});
  await new Promise(resolve=>setTimeout(resolve,10));
  if(expandedSlot!==2)throw new Error('未展開首個錯誤卡片');
  if(focused!=='bus_route_2')throw new Error(`聚焦錯誤：${focused}`);
  process.stdout.write('ok');
})().catch(error=>{console.error(error.message);process.exit(1)});
"""
        completed = subprocess.run(
            ["node", "-e", script + harness],
            cwd=ROOT,
            text=True,
            capture_output=True,
            timeout=10,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(completed.stdout, "ok")

    def test_journey_catalog_loading_blocks_save_and_selected_pair_reaches_post(self):
        source = (ROOT / "src/TransitInkPortalPage.cpp").read_text()
        self.assertIn(".catalog-status::before", source)
        self.assertIn("@keyframes catalog-spin", source)
        script = source.split("<script>", 1)[1].split("</script>", 1)[0]
        script = script.split("byId('config_form').addEventListener", 1)[0]
        harness = r"""
const elements={};
function element(id){return elements[id]||(elements[id]={id,value:'',checked:false,hidden:false,innerHTML:'',textContent:'',disabled:false,selectedOptions:[],setAttribute(){},insertAdjacentHTML(){},focus(){}})}
global.document={getElementById:element,querySelector(){return element('primary')}};
let resolveLocations;
let resolveDestinations;
let postedConfig=null;
global.fetch=(path,options)=>{
  if(path==='/api/catalog/journey/locations')return new Promise(resolve=>{resolveLocations=()=>resolve({ok:true,json:async()=>({data:[{id:'H1',label_tc:'告士打道東行近稅務大樓'}]}),text:async()=>''})});
  if(path.startsWith('/api/catalog/journey/destinations'))return new Promise(resolve=>{resolveDestinations=()=>resolve({ok:true,json:async()=>({data:[{id:'CH',label_tc:'紅磡海底隧道'}]}),text:async()=>''})});
  if(path==='/api/save'){postedConfig=JSON.parse(options.body);return Promise.resolve({ok:true,text:async()=>"設定已儲存"})}
  throw new Error(`未預期要求：${path}`);
};
(async()=>{
  element('wifi_ssid').value='TransitInk';
  element('weather_location').value='香港天文台';
  localCatalog.index={schema_version:1,revision:'fixture123',bus:{kmb_lwb:{routes:{}},ctb:{routes:{}}},gmb:{routes:{}}};
  localCatalog.rail={schema_version:1,revision:'fixture123',modes:{heavy_rail:[],light_rail:[]}};
  widgetDrafts=Array.from({length:widgetSlotCount},()=>emptyWidget());
  widgetDrafts[2].type='journey_time';
  expandedSlot=2;
  renderWidgetCards();
  ensureCatalogForSlot(2);

  let markup=element('widget_cards').innerHTML;
  if(!markup.includes('正在載入指示地點'))throw new Error('指示地點沒有 Loading 文案');
  if(!markup.includes('aria-busy="true"')||!markup.includes('disabled'))throw new Error('Loading 欄位仍可操作');
  await saveConfig({preventDefault(){},submitter:element('submit')});
  if(postedConfig!==null)throw new Error('目錄載入期間仍送出設定');
  if(!element('save_status').textContent.includes('載入'))throw new Error('儲存列沒有載入提示');

  resolveLocations();
  await new Promise(resolve=>setTimeout(resolve,0));
  setJourneyLocation(2,{value:'H1',selectedOptions:[{textContent:'告士打道東行近稅務大樓'}]});
  await Promise.resolve();
  markup=element('widget_cards').innerHTML;
  if(!markup.includes('正在載入行車方向'))throw new Error('行車方向沒有 Loading 文案');

  resolveDestinations();
  await new Promise(resolve=>setTimeout(resolve,0));
  setJourneyDestination(2,{value:'CH',selectedOptions:[{textContent:'紅磡海底隧道'}]});
  await saveConfig({preventDefault(){},submitter:element('submit')});
  const saved=postedConfig?.widgets?.[2];
  if(saved?.type!=='journey_time')throw new Error('行車時間類型沒有送出');
  if(saved.journey_time?.location_id!=='H1'||saved.journey_time?.destination_id!=='CH')throw new Error('行車時間組合沒有送出');
  if(saved.journey_time?.destination_label_tc!=='紅磡海底隧道')throw new Error('行車方向名稱沒有送出');
  process.stdout.write('ok');
})().catch(error=>{console.error(error.message);process.exit(1)});
"""
        completed = subprocess.run(
            ["node", "-e", script + harness],
            cwd=ROOT,
            text=True,
            capture_output=True,
            timeout=10,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(completed.stdout, "ok")

    def test_successful_save_locks_settings_until_the_page_is_reopened(self):
        source = (ROOT / "src/TransitInkPortalPage.cpp").read_text()
        script = source.split("<script>", 1)[1].split("</script>", 1)[0]
        script = script.split("byId('config_form').addEventListener", 1)[0]
        harness = r"""
const elements={};
function element(id){return elements[id]||(elements[id]={id,value:'',checked:false,hidden:false,innerHTML:'',textContent:'',disabled:false,selectedOptions:[],setAttribute(name,value){this[name]=value},insertAdjacentHTML(){},focus(){}})}
const editableControls=[element('wifi_ssid'),element('ui_locale'),element('display_font'),element('catalog_update_button'),element('submit')];
global.document={
  getElementById:element,
  querySelector(){return element('submit')},
  querySelectorAll(selector){return selector.startsWith('#config_form')?editableControls:[]}
};
let saveCalls=0;
global.fetch=(path)=>{
  if(path!=='/api/save')throw new Error(`未預期要求：${path}`);
  saveCalls++;
  return Promise.resolve({ok:true,text:async()=>'saved'});
};
(async()=>{
  portalLocale='en-GB';
  element('wifi_ssid').value='TransitInk';
  element('weather_location').value='Hong Kong Observatory';
  widgetDrafts=Array.from({length:widgetSlotCount},()=>emptyWidget());
  await saveConfig({preventDefault(){},submitter:element('submit')});
  if(saveCalls!==1)throw new Error('第一次儲存沒有送出');
  if(!settingsLocked||editableControls.some(control=>!control.disabled))throw new Error('成功儲存後仍可修改設定');
  if(element('config_form')['data-settings-locked']!=='true')throw new Error('表單沒有標記為已鎖定');
  if(!element('save_status').textContent.includes('reopen the settings page'))throw new Error('沒有提示重新開啟設定頁');
  await saveConfig({preventDefault(){},submitter:element('submit')});
  if(saveCalls!==1)throw new Error('鎖定後仍可再次儲存');
  process.stdout.write('ok');
})().catch(error=>{console.error(error.message);process.exit(1)});
"""
        completed = subprocess.run(
            ["node", "-e", script + harness],
            cwd=ROOT,
            text=True,
            capture_output=True,
            timeout=10,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(completed.stdout, "ok")

    def test_transport_labels_follow_locale_and_preserve_both_languages(self):
        source = (ROOT / "src/TransitInkPortalPage.cpp").read_text()
        script = source.split("<script>", 1)[1].split("</script>", 1)[0]
        script = script.split("byId('config_form').addEventListener", 1)[0]
        harness = r"""
portalLocale='en-GB';
if(localizedLabel({label_tc:'中環',label_en:'Central'})!=='Central')throw new Error('英文名稱未被選用');
if(localizedLabel({label_tc:'馬場',label_en:''})!=='馬場')throw new Error('缺少英文時沒有回退來源語言');
if(localizedLabel({label_tc:'梨木樹',label_en:'LEI MUK SHUE (CIRCULAR)'})!=='Lei Muk Shue (Circular)')throw new Error('九巴方向英文沒有轉成易讀大小寫');
if(localizedLabel({label_tc:'大窩口站',label_en:'TAI WO HAU BBI-TAI WO HAU STATION'})!=='Tai Wo Hau BBI-Tai Wo Hau Station')throw new Error('九巴站名英文沒有保留縮寫');
if(localizedLabel({label_tc:'方向',label_en:'TSUEN WAN WEST STATION to LEI MUK SHUE'})!=='Tsuen Wan West Station to Lei Muk Shue')throw new Error('英文方向連接詞阻止了大小寫整理');
if(localizedLabel({label_tc:'方向',label_en:'MTR KMB LWB DLR BBI HK NT PHASE III'})!=='MTR KMB LWB DLR BBI HK NT Phase III')throw new Error('交通縮寫沒有保留');
const labels={};
setLabelsFromOption(labels,'stop_label',{dataset:{labelTc:'中環碼頭',labelEn:'Central Pier'}});
if(labels.stop_label_tc!=='中環碼頭'||labels.stop_label_en!=='Central Pier')throw new Error('雙語名稱沒有一併保存');
const empty=emptyWidget();
if(!Object.hasOwn(empty.bus,'stop_label_en')||!Object.hasOwn(empty.mtr,'station_label_en'))throw new Error('設定草稿缺少英文欄位');
const hko=weatherLocations.find(item=>item.label_tc==='香港天文台');
if(localizedLabel(hko)!=='Hong Kong Observatory')throw new Error('天氣位置沒有使用官方英文名稱');
const london=weatherLocations.find(item=>item.id==='uk:london');
if(localizedLabel(london)!=='London'||london.region!=='uk')throw new Error('英國天氣位置沒有分層或英文名稱');
if(!timeZones.some(item=>item.id==='Europe/London'))throw new Error('缺少英國時區');
portalLocale='zh-HK';
if(localizedLabel({label_tc:'中環',label_en:'Central'})!=='中環')throw new Error('繁中名稱未被選用');
process.stdout.write('ok');
"""
        completed = subprocess.run(
            ["node", "-e", script + harness],
            cwd=ROOT,
            text=True,
            capture_output=True,
            timeout=10,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(completed.stdout, "ok")

    def test_english_direction_fallback_does_not_show_chinese_connector(self):
        source = (ROOT / "src/TransitInkPortalPage.cpp").read_text()
        script = source.split("<script>", 1)[1].split("</script>", 1)[0]
        script = script.split("byId('config_form').addEventListener", 1)[0]
        harness = r"""
portalLocale='en-GB';
if(localizedField({direction_label_tc:'數碼港 往 鰂魚涌'},'direction_label')!=='數碼港 to 鰂魚涌')throw new Error('已儲存方向仍顯示中文連接詞');
localCatalog.overrides.set('gmb::69',{directions:[{id:'2000410:1',label_tc:'數碼港 往 鰂魚涌',origin_label_tc:'數碼港',destination_label_tc:'鰂魚涌'}]});
const gmb=gmbDirectionItems('69')[0];
if(localizedLabel(gmb)!=='數碼港 to 鰂魚涌'||gmb.label_en.includes('往'))throw new Error('舊小巴快取仍顯示中文連接詞');
localCatalog.overrides.set('bus:kmb:43X',{directions:[{id:'O:2',service_type:'2',label_tc:'荃灣西站 往 青衣碼頭（服務 2）',origin_label_tc:'荃灣西站',destination_label_tc:'青衣碼頭'}]});
const bus=busDirectionItems('kmb','43X')[0];
if(localizedLabel(bus)!=='荃灣西站 to 青衣碼頭 (service 2)'||bus.label_en.includes('往'))throw new Error('舊巴士快取仍顯示中文連接詞');
localCatalog.overrides.set('bus:tfl:24',{directions:[{id:'inbound:490012280A|490015832E',direction_id:'inbound',service_type:'490012280A|490015832E',label_tc:'Pimlico 往 Hampstead Heath',origin_label_tc:'Pimlico',destination_label_tc:'Hampstead Heath'}]});
const tfl=busDirectionItems('tfl','24')[0];
if(localizedLabel(tfl)!=='Pimlico to Hampstead Heath'||tfl.label_en.includes('service')||tfl.label_en.includes('490012280A'))throw new Error('倫敦巴士方向顯示內部服務或站點 ID');
process.stdout.write('ok');
"""
        completed = subprocess.run(
            ["node", "-e", script + harness],
            cwd=ROOT,
            text=True,
            capture_output=True,
            timeout=10,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(completed.stdout, "ok")


if __name__ == "__main__":
    unittest.main()
