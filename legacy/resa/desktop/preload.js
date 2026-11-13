import { contextBridge, ipcRenderer } from 'electron';

// Fixed operations only. Lighting accepts two bounded levels, never raw packets.
contextBridge.exposeInMainWorld('resa', Object.freeze({
  macroStatus: () => ipcRenderer.invoke('macro:status'),
  applyMacro: request => ipcRenderer.invoke('macro:apply',request),
  stopMacro: () => ipcRenderer.invoke('macro:stop'),
  readSession: () => ipcRenderer.invoke('session:read'),
  applySession: request => ipcRenderer.invoke('session:apply',request),
  readDevice: () => ipcRenderer.invoke('device:operation', 'read', {}),
  applyDevice: (request) => ipcRenderer.invoke('device:operation', 'apply', request),
  restoreDevice: (request) => ipcRenderer.invoke('device:operation', 'restore', request),
  applyRGB: (settings) => ipcRenderer.invoke('device:operation', 'rgb', settings),
  discover: () => ipcRenderer.invoke('device:discover'),
  previewLighting: (settings) => ipcRenderer.invoke('lighting:plan', settings),
  applyLighting: (settings) => ipcRenderer.invoke('lighting:apply', settings),
}));
