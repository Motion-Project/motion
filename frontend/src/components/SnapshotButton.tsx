import { useState } from 'react'
import { takeSnapshot } from '@/api/client'
import { useToast } from '@/components/Toast'

interface SnapshotButtonProps {
  cameraId: number
}

export function SnapshotButton({ cameraId }: SnapshotButtonProps) {
  const [isPending, setIsPending] = useState(false)
  const { addToast } = useToast()

  const handleSnapshot = async (e: React.MouseEvent) => {
    e.stopPropagation() // Prevent fullscreen toggle

    if (isPending) return

    setIsPending(true)
    try {
      await takeSnapshot(cameraId)
      addToast('Snapshot captured', 'success')
    } catch (error) {
      addToast(
        error instanceof Error ? error.message : 'Failed to capture snapshot',
        'error'
      )
    } finally {
      setIsPending(false)
    }
  }

  return (
    <button
      type="button"
      onClick={handleSnapshot}
      disabled={isPending}
      className="p-1.5 hover:bg-surface rounded-full transition-colors disabled:opacity-50"
      aria-label="Take snapshot"
      title="Take snapshot"
    >
      {isPending ? (
        <svg className="w-5 h-5 text-gray-400 animate-spin" fill="none" viewBox="0 0 24 24">
          <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4" />
          <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4z" />
        </svg>
      ) : (
        <svg
          className="w-5 h-5 text-gray-400 hover:text-gray-200"
          fill="none"
          stroke="currentColor"
          viewBox="0 0 24 24"
        >
          <path
            strokeLinecap="round"
            strokeLinejoin="round"
            strokeWidth={2}
            d="M3 9a2 2 0 012-2h.93a2 2 0 001.664-.89l.812-1.22A2 2 0 0110.07 4h3.86a2 2 0 011.664.89l.812 1.22A2 2 0 0018.07 7H19a2 2 0 012 2v9a2 2 0 01-2 2H5a2 2 0 01-2-2V9z"
          />
          <path
            strokeLinecap="round"
            strokeLinejoin="round"
            strokeWidth={2}
            d="M15 13a3 3 0 11-6 0 3 3 0 016 0z"
          />
        </svg>
      )}
    </button>
  )
}
