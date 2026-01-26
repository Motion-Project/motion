import { useState } from 'react';
import type { DetectedCamera, PlatformInfo, AddCameraRequest } from '../../api/types';
import { useAddCamera, useTestNetcam } from '../../api/queries';

interface AddCameraWizardProps {
  onClose: () => void;
  detectedCameras: DetectedCamera[];
  platformInfo?: PlatformInfo;
}

type WizardStep = 'select' | 'configure' | 'test' | 'complete';

export default function AddCameraWizard({ onClose, detectedCameras }: AddCameraWizardProps) {
  const [step, setStep] = useState<WizardStep>('select');
  const [selectedCamera, setSelectedCamera] = useState<DetectedCamera | null>(null);
  const [isManualNetcam, setIsManualNetcam] = useState(false);

  // Configuration state
  const [deviceName, setDeviceName] = useState('');
  const [width, setWidth] = useState(0);
  const [height, setHeight] = useState(0);
  const [fps, setFps] = useState(0);
  const [netcamUrl, setNetcamUrl] = useState('');
  const [netcamUser, setNetcamUser] = useState('');
  const [netcamPass, setNetcamPass] = useState('');

  const addCamera = useAddCamera();
  const testNetcam = useTestNetcam();

  const handleSelectCamera = (camera: DetectedCamera) => {
    setSelectedCamera(camera);
    setDeviceName(camera.device_name);
    setWidth(camera.default_width);
    setHeight(camera.default_height);
    setFps(camera.default_fps);
    setStep('configure');
  };

  const handleManualNetcam = () => {
    setIsManualNetcam(true);
    setDeviceName('Network Camera');
    setWidth(1920);
    setHeight(1080);
    setFps(15);
    setStep('configure');
  };

  const handleTestNetcam = async () => {
    if (!netcamUrl) return;

    testNetcam.mutate(
      {
        url: netcamUrl,
        user: netcamUser || undefined,
        pass: netcamPass || undefined,
        timeout: 10,
      },
      {
        onSuccess: (data) => {
          if (data.status === 'ok') {
            setStep('complete');
          }
        },
      }
    );
  };

  const handleAddCamera = () => {
    const request: AddCameraRequest = {
      type: isManualNetcam ? 'netcam' : selectedCamera!.type,
      device_id: isManualNetcam ? netcamUrl : selectedCamera!.device_id,
      device_path: isManualNetcam ? netcamUrl : selectedCamera!.device_path,
      device_name: deviceName,
      sensor_model: selectedCamera?.sensor_model,
      width,
      height,
      fps,
    };

    addCamera.mutate(request, {
      onSuccess: () => {
        setStep('complete');
        setTimeout(() => {
          onClose();
        }, 2000);
      },
    });
  };

  return (
    <div className="fixed inset-0 bg-black/50 flex items-center justify-center z-50 p-4">
      <div className="bg-background rounded-lg shadow-xl max-w-2xl w-full max-h-[90vh] overflow-hidden flex flex-col">
        {/* Header */}
        <div className="p-6 border-b border-secondary/20">
          <div className="flex items-center justify-between">
            <h2 className="text-xl font-semibold text-primary">Add Camera</h2>
            <button
              onClick={onClose}
              className="text-secondary hover:text-primary transition-colors"
            >
              ✕
            </button>
          </div>
          {/* Progress steps */}
          <div className="mt-4 flex items-center gap-2">
            {(['select', 'configure', isManualNetcam ? 'test' : null, 'complete'].filter(Boolean) as WizardStep[]).map(
              (s, idx, arr) => (
                <div key={s} className="flex items-center flex-1">
                  <div
                    className={`flex-1 h-1 rounded ${
                      arr.indexOf(step) >= idx ? 'bg-accent' : 'bg-secondary/20'
                    }`}
                  />
                  {idx < arr.length - 1 && <div className="w-2" />}
                </div>
              )
            )}
          </div>
        </div>

        {/* Content */}
        <div className="flex-1 overflow-y-auto p-6">
          {/* Step 1: Select camera */}
          {step === 'select' && (
            <div className="space-y-4">
              <div>
                <h3 className="text-lg font-medium text-primary mb-2">Select a Camera</h3>
                <p className="text-sm text-secondary">
                  Choose from detected cameras or add a network camera manually
                </p>
              </div>

              {/* Detected cameras */}
              {detectedCameras.length > 0 && (
                <div className="space-y-2">
                  {detectedCameras.map((camera, idx) => (
                    <button
                      key={`${camera.device_id}-${idx}`}
                      onClick={() => handleSelectCamera(camera)}
                      className="w-full p-4 text-left bg-secondary/10 rounded-lg border border-secondary/20 hover:border-accent transition-colors"
                    >
                      <div className="flex items-center gap-3">
                        <div className="text-2xl">
                          {camera.type === 'libcam' ? '🎥' : '📹'}
                        </div>
                        <div className="flex-1">
                          <div className="font-medium text-primary">{camera.device_name}</div>
                          <div className="text-xs text-secondary">
                            {camera.sensor_model && `${camera.sensor_model} • `}
                            {camera.default_width}x{camera.default_height} @ {camera.default_fps}fps
                          </div>
                        </div>
                      </div>
                    </button>
                  ))}
                </div>
              )}

              {/* Manual network camera */}
              <button
                onClick={handleManualNetcam}
                className="w-full p-4 text-left bg-secondary/10 rounded-lg border border-secondary/20 hover:border-accent transition-colors"
              >
                <div className="flex items-center gap-3">
                  <div className="text-2xl">🌐</div>
                  <div className="flex-1">
                    <div className="font-medium text-primary">Add Network Camera</div>
                    <div className="text-xs text-secondary">
                      RTSP, HTTP, or other network camera URL
                    </div>
                  </div>
                </div>
              </button>

              {detectedCameras.length === 0 && (
                <p className="text-sm text-secondary text-center py-4">
                  No cameras detected. Add a network camera manually or connect a camera.
                </p>
              )}
            </div>
          )}

          {/* Step 2: Configure */}
          {step === 'configure' && (
            <div className="space-y-4">
              <div>
                <h3 className="text-lg font-medium text-primary mb-2">Configure Camera</h3>
                <p className="text-sm text-secondary">Set camera name and capture settings</p>
              </div>

              <div className="space-y-3">
                <div>
                  <label className="block text-sm font-medium text-primary mb-1">
                    Camera Name
                  </label>
                  <input
                    type="text"
                    value={deviceName}
                    onChange={(e) => setDeviceName(e.target.value)}
                    className="w-full px-3 py-2 bg-secondary/10 border border-secondary/20 rounded-md text-primary focus:outline-none focus:border-accent"
                    placeholder="e.g., Front Door"
                  />
                </div>

                {isManualNetcam && (
                  <>
                    <div>
                      <label className="block text-sm font-medium text-primary mb-1">
                        Camera URL *
                      </label>
                      <input
                        type="text"
                        value={netcamUrl}
                        onChange={(e) => setNetcamUrl(e.target.value)}
                        className="w-full px-3 py-2 bg-secondary/10 border border-secondary/20 rounded-md text-primary focus:outline-none focus:border-accent font-mono text-sm"
                        placeholder="rtsp://192.168.1.100:554/stream"
                      />
                    </div>

                    <div className="grid grid-cols-2 gap-3">
                      <div>
                        <label className="block text-sm font-medium text-primary mb-1">
                          Username (optional)
                        </label>
                        <input
                          type="text"
                          value={netcamUser}
                          onChange={(e) => setNetcamUser(e.target.value)}
                          className="w-full px-3 py-2 bg-secondary/10 border border-secondary/20 rounded-md text-primary focus:outline-none focus:border-accent"
                          placeholder="admin"
                        />
                      </div>
                      <div>
                        <label className="block text-sm font-medium text-primary mb-1">
                          Password (optional)
                        </label>
                        <input
                          type="password"
                          value={netcamPass}
                          onChange={(e) => setNetcamPass(e.target.value)}
                          className="w-full px-3 py-2 bg-secondary/10 border border-secondary/20 rounded-md text-primary focus:outline-none focus:border-accent"
                          placeholder="••••••"
                        />
                      </div>
                    </div>
                  </>
                )}

                <div className="grid grid-cols-3 gap-3">
                  <div>
                    <label className="block text-sm font-medium text-primary mb-1">
                      Width
                    </label>
                    <input
                      type="number"
                      value={width}
                      onChange={(e) => setWidth(parseInt(e.target.value))}
                      className="w-full px-3 py-2 bg-secondary/10 border border-secondary/20 rounded-md text-primary focus:outline-none focus:border-accent"
                    />
                  </div>
                  <div>
                    <label className="block text-sm font-medium text-primary mb-1">
                      Height
                    </label>
                    <input
                      type="number"
                      value={height}
                      onChange={(e) => setHeight(parseInt(e.target.value))}
                      className="w-full px-3 py-2 bg-secondary/10 border border-secondary/20 rounded-md text-primary focus:outline-none focus:border-accent"
                    />
                  </div>
                  <div>
                    <label className="block text-sm font-medium text-primary mb-1">
                      FPS
                    </label>
                    <input
                      type="number"
                      value={fps}
                      onChange={(e) => setFps(parseInt(e.target.value))}
                      className="w-full px-3 py-2 bg-secondary/10 border border-secondary/20 rounded-md text-primary focus:outline-none focus:border-accent"
                    />
                  </div>
                </div>
              </div>
            </div>
          )}

          {/* Step 3: Test (netcam only) */}
          {step === 'test' && isManualNetcam && (
            <div className="space-y-4">
              <div>
                <h3 className="text-lg font-medium text-primary mb-2">Test Connection</h3>
                <p className="text-sm text-secondary">
                  Verify that the camera URL is accessible
                </p>
              </div>

              <div className="p-4 bg-secondary/10 rounded-lg">
                <div className="text-sm font-mono text-primary break-all">{netcamUrl}</div>
              </div>

              {testNetcam.isError && (
                <div className="p-3 bg-red-500/10 border border-red-500/20 rounded-lg text-sm text-red-600 dark:text-red-400">
                  Connection test failed. Check the URL and credentials.
                </div>
              )}

              {testNetcam.isSuccess && testNetcam.data.status === 'ok' && (
                <div className="p-3 bg-green-500/10 border border-green-500/20 rounded-lg text-sm text-green-600 dark:text-green-400">
                  ✓ Connection successful!
                </div>
              )}
            </div>
          )}

          {/* Step 4: Complete */}
          {step === 'complete' && (
            <div className="text-center space-y-4 py-8">
              <div className="text-6xl">✓</div>
              <h3 className="text-lg font-medium text-primary">Camera Added!</h3>
              <p className="text-sm text-secondary">
                {deviceName} has been added to your configuration.
              </p>
            </div>
          )}
        </div>

        {/* Footer */}
        {step !== 'complete' && (
          <div className="p-6 border-t border-secondary/20 flex justify-between">
            <button
              onClick={() => {
                if (step === 'configure') setStep('select');
                else if (step === 'test') setStep('configure');
                else onClose();
              }}
              className="px-4 py-2 text-sm bg-secondary/20 text-primary rounded-md hover:bg-secondary/30 transition-colors"
            >
              Back
            </button>

            <button
              onClick={() => {
                if (step === 'configure') {
                  if (isManualNetcam) {
                    setStep('test');
                  } else {
                    handleAddCamera();
                  }
                } else if (step === 'test') {
                  handleTestNetcam();
                }
              }}
              disabled={
                (step === 'configure' && (!deviceName || !width || !height || !fps)) ||
                (step === 'configure' && isManualNetcam && !netcamUrl) ||
                (step === 'test' && testNetcam.isPending) ||
                addCamera.isPending
              }
              className="px-4 py-2 text-sm bg-accent text-white rounded-md hover:bg-accent/90 transition-colors disabled:opacity-50 disabled:cursor-not-allowed"
            >
              {step === 'test' && testNetcam.isPending
                ? 'Testing...'
                : step === 'test'
                ? 'Test Connection'
                : addCamera.isPending
                ? 'Adding...'
                : step === 'configure' && !isManualNetcam
                ? 'Add Camera'
                : 'Next'}
            </button>
          </div>
        )}
      </div>
    </div>
  );
}
