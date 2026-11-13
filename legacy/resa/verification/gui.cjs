// All window interaction and captures run only inside tools/virtual-screen.py.
const {app,BrowserWindow}=require('electron');
const fs=require('node:fs/promises');
const path=require('node:path');
const assert=require('node:assert/strict');
if(process.env.KUBUNTU_VIRTUAL_SCREEN!=='1'||!process.env.XDG_RUNTIME_DIR?.includes('codex-virtual-screen-'))throw Error('Use the private virtual-screen runner');
const output=process.env.GM320_TEST_OUTPUT;
if(!output||!path.isAbsolute(output))throw Error('Provide an absolute test artifact directory');
async function waitFor(fn,timeout=15000){const end=Date.now()+timeout;while(Date.now()<end){const value=await fn();if(value)return value;await new Promise(r=>setTimeout(r,600));}throw Error('UI did not become ready');}
(async()=>{
  await fs.mkdir(output,{recursive:true});
  await import(require('node:url').pathToFileURL(path.join(process.env.GM320_TEST_APP_DIR||path.resolve(__dirname,'..'),'desktop/main.js')).href);
  await app.whenReady();
  const win=await waitFor(()=>BrowserWindow.getAllWindows()[0]);
  const errors=[];win.webContents.on('console-message',(_event,level,message)=>{if(level>=2)errors.push(message);});
  await waitFor(()=>win.webContents.executeJavaScript("document.getElementById('save-status')?.textContent.includes('Mouse connected')"));
  const initial=await win.webContents.executeJavaScript(`({title:document.title,connection:document.getElementById('connection-label').textContent,settings:document.getElementById('save-status').textContent,buttons:Array.from(document.querySelectorAll('#assign-left select,#assign-right select')).map(s=>s.value),applyDisabled:document.getElementById('save').disabled})`);
  initial.electronVersion=process.versions.electron;
  assert.equal(initial.title,'Resa — Ant Esports GM320');assert.equal(initial.buttons[6],'Disabled');assert(initial.applyDisabled);
  await new Promise(r=>setTimeout(r,600));
  await fs.writeFile(path.join(output,'buttons.png'),(await win.webContents.capturePage()).toPNG());
  await win.webContents.executeJavaScript("document.getElementById('tab-sensitivity').click()");
  await new Promise(r=>setTimeout(r,600));
  await fs.writeFile(path.join(output,'sensitivity.png'),(await win.webContents.capturePage()).toPNG());
  await win.webContents.executeJavaScript("document.getElementById('tab-macros').click()");
  await win.webContents.executeJavaScript(`document.getElementById('event-key').value='Ctrl';document.getElementById('event-key').dispatchEvent(new Event('change'));document.getElementById('add-event').click();document.getElementById('event-key').value='C';document.getElementById('event-key').dispatchEvent(new Event('change'));document.getElementById('add-event').click();document.getElementById('event-type').value='up';document.getElementById('event-type').dispatchEvent(new Event('change'));document.getElementById('add-event').click();document.getElementById('event-key').value='Ctrl';document.getElementById('event-key').dispatchEvent(new Event('change'));document.getElementById('add-event').click();document.getElementById('save-preset').click();`);
  const macro=await win.webContents.executeJavaScript(`({count:document.getElementById('macro-count').textContent,presetSaved:document.getElementById('save-preset').disabled,stored:JSON.parse(localStorage.getItem('resa.gm320.profiles.v1')).profiles[0].macro})`);
  assert.equal(macro.count,'4 / 64');assert(macro.presetSaved);assert.equal(macro.stored.length,4);
  const macroStatus=await win.webContents.executeJavaScript('window.resa.macroStatus()');
  assert.equal(macroStatus.installed,true);assert.equal(macroStatus.active,false);
  await new Promise(r=>setTimeout(r,600));
  await fs.writeFile(path.join(output,'macros.png'),(await win.webContents.capturePage()).toPNG());
  await win.webContents.executeJavaScript("document.getElementById('tab-lighting').click()");
  await new Promise(r=>setTimeout(r,600));
  await fs.writeFile(path.join(output,'lighting.png'),(await win.webContents.capturePage()).toPNG());
  await fs.writeFile(path.join(output,'result.json'),JSON.stringify({initial,macro,macroStatus,errors,production_settings_writes:0,input_injection:0},null,2)+'\n');
  // Saved macro edits have a distinct local preset path; no Apply was clicked.
  win.destroy();app.quit();
})().catch(error=>{console.error(error.stack);app.exit(1);});
