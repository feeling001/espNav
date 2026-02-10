import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

export default defineConfig({
  plugins: [react()],
  build: {
    outDir: '../data/www',
    emptyOutDir: true,
    target: 'es2015',
    minify: 'terser',
    terserOptions: {
      compress: {
        drop_console: true,
        dead_code: true
      }
    },
    rollupOptions: {
      output: {
        manualChunks: undefined
      }
    },
    cssMinify: true,
    assetsInlineLimit: 4096
  }
})
