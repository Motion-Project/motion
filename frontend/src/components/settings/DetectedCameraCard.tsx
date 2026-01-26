import type { DetectedCamera } from '../../api/types';

interface DetectedCameraCardProps {
  camera: DetectedCamera;
  onAdd: () => void;
}

export default function DetectedCameraCard({ camera, onAdd }: DetectedCameraCardProps) {
  const typeLabel = {
    libcam: 'Pi Camera (libcamera)',
    v4l2: 'USB Camera (V4L2)',
    netcam: 'Network Camera',
  }[camera.type];

  const typeIcon = {
    libcam: '🎥',
    v4l2: '📹',
    netcam: '🌐',
  }[camera.type];

  return (
    <div className="p-4 bg-secondary/10 rounded-lg border border-secondary/20 hover:border-accent/50 transition-colors">
      <div className="flex items-start justify-between">
        <div className="flex items-start gap-3 flex-1">
          <div className="text-2xl">{typeIcon}</div>
          <div className="flex-1 min-w-0">
            <div className="flex items-center gap-2">
              <h5 className="font-medium text-primary">{camera.device_name}</h5>
              <span className="text-xs px-2 py-0.5 bg-accent/20 text-accent rounded">
                {typeLabel}
              </span>
            </div>

            <dl className="mt-2 grid grid-cols-2 gap-x-4 gap-y-1 text-xs">
              {camera.sensor_model && (
                <>
                  <dt className="text-secondary">Sensor:</dt>
                  <dd className="text-primary font-mono">{camera.sensor_model}</dd>
                </>
              )}
              <dt className="text-secondary">Device:</dt>
              <dd className="text-primary font-mono truncate" title={camera.device_path}>
                {camera.device_path}
              </dd>
              <dt className="text-secondary">Default:</dt>
              <dd className="text-primary">
                {camera.default_width}x{camera.default_height} @ {camera.default_fps}fps
              </dd>
            </dl>

            {camera.resolutions.length > 0 && (
              <details className="mt-2">
                <summary className="text-xs text-secondary cursor-pointer hover:text-primary">
                  Available resolutions ({camera.resolutions.length})
                </summary>
                <div className="mt-1 flex flex-wrap gap-1">
                  {camera.resolutions.slice(0, 10).map(([w, h], idx) => (
                    <span key={idx} className="text-xs px-1.5 py-0.5 bg-secondary/10 rounded">
                      {w}x{h}
                    </span>
                  ))}
                  {camera.resolutions.length > 10 && (
                    <span className="text-xs text-secondary">
                      +{camera.resolutions.length - 10} more
                    </span>
                  )}
                </div>
              </details>
            )}
          </div>
        </div>

        <button
          onClick={onAdd}
          className="ml-4 px-3 py-1.5 text-sm bg-accent text-white rounded hover:bg-accent/90 transition-colors whitespace-nowrap"
        >
          Add Camera
        </button>
      </div>
    </div>
  );
}
