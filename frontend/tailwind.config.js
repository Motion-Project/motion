/** @type {import('tailwindcss').Config} */
export default {
  content: [
    "./index.html",
    "./src/**/*.{js,ts,jsx,tsx}",
  ],
  theme: {
    extend: {
      colors: {
        surface: {
          DEFAULT: '#1a1a1a',
          elevated: '#262626',
          hover: '#2e2e2e',
        },
        primary: {
          DEFAULT: '#3b82f6',
          hover: '#2563eb',
        },
        danger: {
          DEFAULT: '#ef4444',
          hover: '#dc2626',
        },
      },
    },
  },
  plugins: [],
}
