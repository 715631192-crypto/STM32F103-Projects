/**
 * main.js — Vue 3 entry
 * HBuilder X + uni-app Vue 3 风格
 */
import App from './App.vue';
import { createSSRApp } from 'vue';

export function createApp() {
  const app = createSSRApp(App);
  return { app };
}
