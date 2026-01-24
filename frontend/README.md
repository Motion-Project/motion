# Motion Web UI - Frontend

Modern React-based web interface for the Motion video surveillance system. Built with React 19, TypeScript, TanStack Query, and Tailwind CSS.

## Technology Stack

| Layer | Technology | Purpose |
|-------|------------|---------|
| Framework | React 19 | UI components and rendering |
| Language | TypeScript 5.9 | Type safety |
| Build Tool | Vite 7 | Fast development and optimized builds |
| Styling | Tailwind CSS 3.4 | Utility-first CSS |
| State | Zustand 5 | Lightweight global state |
| Data Fetching | TanStack Query 5 | Server state, caching, mutations |
| Forms | React Hook Form + Zod | Form validation |
| Routing | React Router 7 | Client-side routing |

## Quick Start

```bash
# Install dependencies
npm install

# Development server (http://localhost:5173)
npm run dev

# Production build (outputs to ../data/webui/)
npm run build

# Type checking
npx tsc --noEmit

# Linting
npm run lint
```

## Project Structure

```
frontend/
├── src/
│   ├── api/                 # API client and data fetching
│   │   ├── client.ts        # HTTP client with retry, timeout, CSRF
│   │   ├── csrf.ts          # CSRF token management
│   │   ├── queries.ts       # TanStack Query hooks
│   │   ├── types.ts         # API response types
│   │   ├── profiles.ts      # Profile management API
│   │   └── system.ts        # System status API
│   │
│   ├── components/          # React components
│   │   ├── form/            # Reusable form controls
│   │   │   ├── FormInput.tsx
│   │   │   ├── FormSelect.tsx
│   │   │   ├── FormSlider.tsx
│   │   │   ├── FormToggle.tsx
│   │   │   └── FormSection.tsx
│   │   │
│   │   ├── settings/        # Settings panel components
│   │   │   ├── DeviceSettings.tsx
│   │   │   ├── LibcameraSettings.tsx
│   │   │   ├── MotionSettings.tsx
│   │   │   ├── MovieSettings.tsx
│   │   │   ├── PictureSettings.tsx
│   │   │   ├── StreamSettings.tsx
│   │   │   └── ...
│   │   │
│   │   ├── Layout.tsx       # App shell with nav and system status
│   │   ├── CameraStream.tsx # MJPEG stream display with FPS tracking
│   │   ├── BottomSheet.tsx  # Mobile-friendly slide-up panel
│   │   ├── QuickSettings.tsx # Quick camera adjustments
│   │   ├── LoginModal.tsx   # Authentication dialog
│   │   ├── Toast.tsx        # Notification system
│   │   └── ErrorBoundary.tsx
│   │
│   ├── contexts/            # React context providers
│   │   └── AuthContext.tsx  # Authentication state management
│   │
│   ├── hooks/               # Custom React hooks
│   │   ├── useCameraStream.ts      # MJPEG stream URL management
│   │   ├── useCameraCapabilities.ts # Camera hardware capability detection
│   │   ├── useProfiles.ts          # Profile CRUD operations
│   │   └── useSheetGestures.ts     # Touch gestures for bottom sheet
│   │
│   ├── lib/                 # Utility libraries
│   │   └── validation.ts    # Zod schemas for form validation
│   │
│   ├── pages/               # Route components
│   │   ├── Dashboard.tsx    # Camera grid view
│   │   ├── Settings.tsx     # Full settings panel (in Layout)
│   │   └── Media.tsx        # Pictures and movies browser
│   │
│   ├── utils/               # Helper functions
│   │   ├── parameterMappings.ts # Motion config param definitions
│   │   └── translations.ts      # Unit conversions (pixels <-> %)
│   │
│   ├── App.tsx              # Route definitions
│   ├── main.tsx             # App entry point with providers
│   └── index.css            # Tailwind imports and custom styles
│
├── public/                  # Static assets
├── index.html               # HTML template
├── vite.config.ts           # Vite configuration
├── tailwind.config.js       # Tailwind customization
└── tsconfig.json            # TypeScript configuration
```

## Architecture

### Data Flow

```
┌─────────────────────────────────────────────────────────────┐
│                        React Components                      │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    TanStack Query Hooks                      │
│  useMotionConfig(), useCameras(), useBatchUpdateConfig()    │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      API Client Layer                        │
│  apiGet(), apiPost(), apiPatch(), apiDelete()               │
│  - Automatic retry for 502/503/504                          │
│  - CSRF token refresh on 403                                │
│  - Request timeout (10s)                                    │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                     Motion C++ Backend                       │
│  /0/api/config, /{cam}/api/config, /{cam}/mjpg/stream      │
└─────────────────────────────────────────────────────────────┘
```

### Provider Hierarchy

```tsx
<React.StrictMode>
  <ErrorBoundary>
    <QueryClientProvider>      // TanStack Query cache
      <ToastProvider>          // Notification system
        <BrowserRouter>        // React Router
          <AuthProvider>       // Authentication state
            <App />
            <LoginModal />
          </AuthProvider>
        </BrowserRouter>
      </ToastProvider>
    </QueryClientProvider>
  </ErrorBoundary>
</React.StrictMode>
```

## API Integration

### Endpoints

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/0/api/config` | GET | Full config + camera list + CSRF token |
| `/0/api/cameras` | GET | Camera list only |
| `/{cam}/api/config` | PATCH | Update config parameters |
| `/{cam}/mjpg/stream` | GET | MJPEG video stream |
| `/{cam}/api/media/pictures` | GET | List captured pictures |
| `/{cam}/api/media/movies` | GET | List recorded movies |
| `/0/api/system/status` | GET | CPU temp, memory, disk usage |
| `/0/api/auth/me` | GET | Authentication status |

### Query Keys

```typescript
const queryKeys = {
  config: (camId?: number) => ['config', camId],
  cameras: ['cameras'],
  pictures: (camId: number) => ['pictures', camId],
  movies: (camId: number) => ['movies', camId],
  temperature: ['temperature'],
  systemStatus: ['systemStatus'],
  auth: ['auth'],
}
```

### CSRF Protection

The API client automatically:
1. Stores CSRF token from `/0/api/config` response
2. Includes token in `X-CSRF-Token` header for POST/PATCH/DELETE
3. Refreshes token on 403 response (Motion may have restarted)

## Components

### Form Components (`src/components/form/`)

Reusable form controls with consistent styling:

| Component | Purpose |
|-----------|---------|
| `FormInput` | Text/number input with label and help text |
| `FormSelect` | Dropdown selection |
| `FormToggle` | Boolean switch |
| `FormSlider` | Range input with live value display |
| `FormSection` | Collapsible settings group |

### Settings Components (`src/components/settings/`)

Settings panels organized by category:

| Component | Motion Parameters |
|-----------|------------------|
| `DeviceSettings` | `device_name`, `width`, `height`, `framerate`, `rotate` |
| `LibcameraSettings` | `libcam_brightness`, `libcam_contrast`, `libcam_gain`, AWB, AF (conditional on camera capabilities) |
| `MotionSettings` | `threshold`, `noise_level`, `event_gap`, masks |
| `StreamSettings` | `stream_quality`, `stream_maxrate`, `stream_motion` |
| `MovieSettings` | `movie_output`, `movie_quality`, `movie_container`, `movie_encoder_preset`, hardware encoding options |
| `PictureSettings` | `picture_output`, `picture_quality`, `snapshot_interval` |
| `OverlaySettings` | `text_left`, `text_right`, `text_scale` |
| `StorageSettings` | `target_dir` |
| `ScheduleSettings` | `schedule_params` |

## Hooks

### `useCameraStream(cameraId)`

Manages MJPEG stream URL and connection state:

```typescript
const { streamUrl, isLoading, error } = useCameraStream(1)
```

### `useProfiles(cameraId)`

Profile management (save/load/delete configuration presets):

```typescript
const { profiles, saveProfile, loadProfile, deleteProfile } = useProfiles(1)
```

### `useCameraCapabilities(cameraId)`

Fetches camera hardware capabilities from `status.json`. Used to conditionally render controls based on camera features (e.g., hide autofocus for Pi Camera v2):

```typescript
const { data: capabilities } = useCameraCapabilities(1)

// Check specific capabilities
if (capabilities?.AfMode) {
  // Show autofocus controls
}
```

Capabilities include:
- `AfMode`, `LensPosition`, `AfRange`, `AfSpeed` - Autofocus
- `AwbEnable`, `AwbMode`, `ColourGains` - White balance
- `Brightness`, `Contrast`, `Saturation`, `Sharpness` - Image controls

## Styling

### Color Tokens (Tailwind)

```javascript
// tailwind.config.js
colors: {
  surface: '#1a1a1a',           // Background
  'surface-elevated': '#242424', // Cards, panels
  primary: '#3b82f6',           // Accent color
  danger: '#ef4444',            // Errors
}
```

### Responsive Design

- Mobile-first approach with Tailwind breakpoints
- `BottomSheet` component for touch-friendly mobile settings
- Adaptive grid layouts for camera dashboard
- Collapsible navigation on mobile

## Development

### Path Aliases

Configured in `vite.config.ts` and `tsconfig.json`:

```typescript
import { CameraStream } from '@/components/CameraStream'
import { useCameras } from '@/api/queries'
```

### Adding a New Setting

1. Add parameter to `src/utils/parameterMappings.ts`
2. Create or update component in `src/components/settings/`
3. Use form components with `useBatchUpdateConfig()` mutation
4. Handle debouncing for sliders (300ms default)

### Build Output

Production build outputs to `../data/webui/` for installation:

```
data/webui/
├── index.html
└── assets/
    ├── index-[hash].js
    └── index-[hash].css
```

## Performance Considerations

- **Query caching**: 30-60 second stale times to reduce API calls
- **Debounced updates**: Slider changes batched before API call
- **Optimistic updates**: UI updates immediately, reverts on error
- **Lazy loading**: Settings fetched only when panel opened
- **FPS tracking**: Lightweight calculation using `naturalWidth` changes

## Authentication

Motion supports HTTP Basic/Digest authentication. The UI:

1. Checks auth status on load via `/0/api/auth/me`
2. Shows login modal on 401/403 responses
3. Hides admin-only features (Settings) for non-admin users
4. Stores auth state in `AuthContext`
