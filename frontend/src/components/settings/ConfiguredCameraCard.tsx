import { useState } from 'react';
import type { Camera } from '../../api/types';
import { useDeleteCamera } from '../../api/queries';

interface ConfiguredCameraCardProps {
  camera: Camera;
}

export default function ConfiguredCameraCard({ camera }: ConfiguredCameraCardProps) {
  const [showConfirm, setShowConfirm] = useState(false);
  const deleteCamera = useDeleteCamera();

  const handleDelete = () => {
    deleteCamera.mutate({ camId: camera.id }, {
      onSuccess: () => {
        setShowConfirm(false);
      },
    });
  };

  return (
    <div className="p-4 bg-secondary/10 rounded-lg border border-secondary/20">
      <div className="flex items-start justify-between">
        <div className="flex items-start gap-3 flex-1">
          <div className="text-2xl">✅</div>
          <div className="flex-1 min-w-0">
            <div className="flex items-center gap-2">
              <h5 className="font-medium text-primary">{camera.name}</h5>
              <span className="text-xs px-2 py-0.5 bg-green-500/20 text-green-600 dark:text-green-400 rounded">
                Active
              </span>
            </div>

            <dl className="mt-2 grid grid-cols-2 gap-x-4 gap-y-1 text-xs">
              <dt className="text-secondary">Camera ID:</dt>
              <dd className="text-primary font-mono">{camera.id}</dd>
              {camera.width && camera.height && (
                <>
                  <dt className="text-secondary">Resolution:</dt>
                  <dd className="text-primary">
                    {camera.width}x{camera.height}
                  </dd>
                </>
              )}
              <dt className="text-secondary">Stream URL:</dt>
              <dd className="text-primary font-mono truncate">
                <a
                  href={camera.url}
                  target="_blank"
                  rel="noopener noreferrer"
                  className="hover:text-accent"
                >
                  {camera.url}
                </a>
              </dd>
            </dl>
          </div>
        </div>

        <div className="ml-4 flex gap-2">
          <a
            href={`/camera/${camera.id}`}
            className="px-3 py-1.5 text-sm bg-secondary/20 text-primary rounded hover:bg-secondary/30 transition-colors"
          >
            View
          </a>
          {!showConfirm ? (
            <button
              onClick={() => setShowConfirm(true)}
              className="px-3 py-1.5 text-sm bg-red-500/20 text-red-600 dark:text-red-400 rounded hover:bg-red-500/30 transition-colors"
            >
              Remove
            </button>
          ) : (
            <div className="flex gap-1">
              <button
                onClick={handleDelete}
                disabled={deleteCamera.isPending}
                className="px-3 py-1.5 text-sm bg-red-500 text-white rounded hover:bg-red-600 transition-colors disabled:opacity-50"
              >
                {deleteCamera.isPending ? 'Removing...' : 'Confirm'}
              </button>
              <button
                onClick={() => setShowConfirm(false)}
                className="px-3 py-1.5 text-sm bg-secondary/20 text-primary rounded hover:bg-secondary/30 transition-colors"
              >
                Cancel
              </button>
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
