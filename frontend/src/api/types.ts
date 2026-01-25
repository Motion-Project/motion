// Camera from Motion's config
export interface Camera {
  id: number;
  name: string;
  url: string;
  width?: number;
  height?: number;
  all_xpct_st?: number;
  all_xpct_en?: number;
  all_ypct_st?: number;
  all_ypct_en?: number;
}

// Cameras list response from /0/config
export interface CamerasResponse {
  count: number;
  [key: string]: Camera | number;
}

// Full config response from /0/config
export interface MotionConfig {
  csrf_token?: string;
  version: string;
  cameras: CamerasResponse;
  configuration: {
    [key: string]: unknown;
  };
  categories: {
    [key: string]: {
      name: string;
      display: string;
    };
  };
}

// Single config parameter value
export interface ConfigParam {
  name: string;
  value: string | number | boolean;
  category?: string;
  type?: 'string' | 'number' | 'boolean' | 'list';
}

// Media item (snapshot or movie)
export interface MediaItem {
  id: number;
  filename: string;
  path: string;
  date: string;
  time?: string;
  size: number;
  thumbnail?: string; // Optional thumbnail URL for videos
}

// Pagination metadata
export interface PaginationMeta {
  total_count: number;
  offset: number;
  limit: number;
  date_filter: string | null;
}

// Pictures API response from /{cam}/api/media/pictures
export interface PicturesResponse extends PaginationMeta {
  pictures: MediaItem[];
}

// Movies API response from /{cam}/api/media/movies
export interface MoviesResponse extends PaginationMeta {
  movies: MediaItem[];
}

// Date count entry
export interface DateCount {
  date: string;      // YYYYMMDD format
  count: number;
}

// Date summary response from /{cam}/api/media/dates
export interface DateSummaryResponse {
  type: 'pic' | 'movie';
  total_count: number;
  dates: DateCount[];
}

// Folder item from /{cam}/api/media/folders
export interface FolderItem {
  name: string;
  path: string;
  file_count: number;
  total_size: number;
}

// File item from folder browsing
export interface FolderFileItem {
  id: number;
  filename: string;
  path: string;
  type: 'movie' | 'picture';
  date: string;
  time: string;
  size: number;
  thumbnail?: string;
}

// Folder contents response from /{cam}/api/media/folders
export interface FolderContentsResponse {
  path: string;
  parent: string | null;
  folders: FolderItem[];
  files: FolderFileItem[];
  total_files: number;
  offset: number;
  limit: number;
}

// Delete folder files response from DELETE /{cam}/api/media/folders/files
export interface DeleteFolderFilesResponse {
  success: boolean;
  deleted: {
    movies: number;
    pictures: number;
    thumbnails: number;
  };
  errors: string[];
  path: string;
}

// System temperature response from /0/api/system/temperature
export interface TemperatureResponse {
  celsius: number;
  fahrenheit: number;
}

// Camera type discriminator
export type CameraType = 'libcam' | 'v4l2' | 'netcam' | 'unknown';

// V4L2 control structure (matches backend ctx_v4l2ctrl_item)
export interface V4L2Control {
  name: string;
  id: string;
  type: 'integer' | 'boolean' | 'menu';
  min: number;
  max: number;
  default: number;
  current: number;
  step?: number;
  menuItems?: Array<{ value: number; label: string }>;
}

// NETCAM connection status
export type NetcamConnectionStatus = 'connected' | 'reading' | 'not_connected' | 'reconnecting' | 'unknown';

// Per-camera status from /0/api/system/status
export interface CameraStatus {
  name: string;
  id: number;
  width: number;
  height: number;
  fps: number;
  current_time: string;
  missing_frame_counter: number;
  lost_connection: boolean;
  connection_lost_time: string;
  detecting: boolean;
  pause: boolean;
  user_pause: string;
  // Type-specific fields
  camera_type: CameraType;
  camera_device: string;
  supportedControls?: CameraCapabilities;  // libcam only
  v4l2_controls?: V4L2Control[];           // v4l2 only
  netcam_status?: NetcamConnectionStatus;  // netcam only
  has_high_stream?: boolean;               // netcam only
}

// Status section with dynamic camera keys
export interface StatusSection {
  count: number;
  [key: `cam${number}`]: CameraStatus;
}

// System status response from /0/api/system/status
export interface SystemStatus {
  version: string;
  status: StatusSection;
  temperature?: {
    celsius: number;
    fahrenheit: number;
  };
  uptime?: {
    seconds: number;
    days: number;
    hours: number;
  };
  memory?: {
    total: number;
    used: number;
    free: number;
    available: number;
    percent: number;
  };
  disk?: {
    total: number;
    used: number;
    free: number;
    available: number;
    percent: number;
  };
  actions?: {
    service: boolean;
    power: boolean;
  };
}

// API error
export interface ApiError {
  message: string;
  status?: number;
}

// Camera control capabilities from supportedControls in status.json
export interface CameraCapabilities {
  // Autofocus
  AfMode?: boolean;
  LensPosition?: boolean;
  AfTrigger?: boolean;
  AfRange?: boolean;
  AfSpeed?: boolean;
  AfMetering?: boolean;
  // Exposure
  ExposureTime?: boolean;
  ExposureValue?: boolean;
  AnalogueGain?: boolean;
  AeEnable?: boolean;
  AeMeteringMode?: boolean;
  AeConstraintMode?: boolean;
  AeExposureMode?: boolean;
  // White Balance
  AwbEnable?: boolean;
  AwbMode?: boolean;
  AwbLocked?: boolean;
  ColourGains?: boolean;
  ColourTemperature?: boolean;
  // Image Controls
  Brightness?: boolean;
  Contrast?: boolean;
  Saturation?: boolean;
  Sharpness?: boolean;
  // Other
  DigitalGain?: boolean;
  ScalerCrop?: boolean;
}
