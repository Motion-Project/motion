import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import path from 'path'

// https://vite.dev/config/
export default defineConfig({
  plugins: [react()],
  build: {
    outDir: '../data/webui',
    emptyOutDir: true,
  },
  server: {
    port: 5173,
    proxy: {
      '/api': {
        target: 'http://localhost:7999',
        changeOrigin: true,
      },
      '/0': {
        target: 'http://localhost:7999',
        changeOrigin: true,
      },
      '/1': {
        target: 'http://localhost:7999',
        changeOrigin: true,
      },
      '/2': {
        target: 'http://localhost:7999',
        changeOrigin: true,
      },
      '/3': {
        target: 'http://localhost:7999',
        changeOrigin: true,
      },
      '/4': {
        target: 'http://localhost:7999',
        changeOrigin: true,
      },
    },
  },
  resolve: {
    alias: {
      '@': path.resolve(__dirname, './src'),
    },
  },
})
