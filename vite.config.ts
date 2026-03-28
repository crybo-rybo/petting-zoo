import { defineConfig } from 'vite'
import { svelte } from '@sveltejs/vite-plugin-svelte'

const apiTarget = process.env.VITE_API_URL ?? 'http://localhost:8080'

export default defineConfig({
  plugins: [svelte()],
  server: {
    proxy: {
      '/healthz': apiTarget,
      '/v1': apiTarget,
      '/metrics': apiTarget,
    },
  },
})
