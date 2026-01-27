import { useState } from 'react';
import { useCameras, usePlatformInfo, useDetectedCameras } from '../../api/queries';
import AddCameraWizard from './AddCameraWizard';
import DetectedCameraCard from './DetectedCameraCard';
import ConfiguredCameraCard from './ConfiguredCameraCard';

export default function CameraManagement() {
  const [showWizard, setShowWizard] = useState(false);
  const { data: cameras = [], isLoading: camerasLoading } = useCameras();
  const { data: platformInfo, isLoading: platformLoading } = usePlatformInfo();
  const { data: detectedData, isLoading: detectedLoading, refetch: refetchDetected } = useDetectedCameras();

  const detectedCameras = detectedData?.cameras || [];
  const hasDetectedCameras = detectedCameras.length > 0;

  if (camerasLoading || platformLoading) {
    return (
      <div className="p-6 bg-secondary/20 rounded-lg">
        <p className="text-secondary">Loading camera information...</p>
      </div>
    );
  }

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h3 className="text-lg font-semibold text-primary">Camera Management</h3>
          <p className="text-sm text-secondary">
            Add, remove, and configure cameras
            {platformInfo?.is_raspberry_pi && ` • ${platformInfo.pi_model}`}
          </p>
        </div>
        <button
          onClick={() => {
            refetchDetected();
            setShowWizard(true);
          }}
          className="px-4 py-2 bg-accent text-white rounded-md hover:bg-accent/90 transition-colors"
        >
          Add Camera
        </button>
      </div>

      {/* First-run experience */}
      {cameras.length === 0 && (
        <div className="p-8 bg-primary/10 rounded-lg text-center space-y-4">
          <div className="text-4xl">📷</div>
          <h3 className="text-xl font-semibold text-primary">Welcome to Motion</h3>
          <p className="text-secondary max-w-md mx-auto">
            Get started by adding your first camera. Motion will automatically detect connected cameras
            or you can manually configure a network camera.
          </p>
          <button
            onClick={() => {
              refetchDetected();
              setShowWizard(true);
            }}
            className="px-6 py-3 bg-accent text-white rounded-md hover:bg-accent/90 transition-colors font-medium"
          >
            Add Your First Camera
          </button>
        </div>
      )}

      {/* Configured cameras */}
      {cameras.length > 0 && (
        <div className="space-y-3">
          <h4 className="text-sm font-medium text-secondary">Configured Cameras</h4>
          <div className="grid gap-3">
            {cameras.map((camera) => (
              <ConfiguredCameraCard key={camera.id} camera={camera} />
            ))}
          </div>
        </div>
      )}

      {/* Detected cameras */}
      {!detectedLoading && hasDetectedCameras && (
        <div className="space-y-3">
          <h4 className="text-sm font-medium text-secondary">
            Available Cameras ({detectedCameras.length})
          </h4>
          <p className="text-xs text-secondary">
            These cameras were detected but not yet configured
          </p>
          <div className="grid gap-3">
            {detectedCameras.map((camera, index) => (
              <DetectedCameraCard
                key={`${camera.device_id}-${index}`}
                camera={camera}
                onAdd={() => setShowWizard(true)}
              />
            ))}
          </div>
        </div>
      )}

      {/* Add camera wizard */}
      {showWizard && (
        <AddCameraWizard
          onClose={() => setShowWizard(false)}
          detectedCameras={detectedCameras}
          platformInfo={platformInfo}
        />
      )}
    </div>
  );
}
