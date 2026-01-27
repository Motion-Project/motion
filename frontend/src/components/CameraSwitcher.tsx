import type { Camera } from '@/api/types'

interface CameraSwitcherProps {
  cameras: Camera[]
  selectedId: number | null
  onSelect: (id: number) => void
}

export function CameraSwitcher({ cameras, selectedId, onSelect }: CameraSwitcherProps) {
  // No switcher needed for single camera
  if (cameras.length <= 1) return null

  return (
    <select
      value={selectedId ?? ''}
      onChange={(e) => onSelect(Number(e.target.value))}
      className="px-3 py-1.5 bg-surface-elevated border border-gray-700
                 rounded-lg text-sm focus:border-primary focus:ring-1
                 focus:ring-primary cursor-pointer"
      aria-label="Select camera"
    >
      {cameras.map((cam) => (
        <option key={cam.id} value={cam.id}>
          {cam.name}
        </option>
      ))}
    </select>
  )
}
