import { useState, useRef, useCallback, useEffect, useMemo } from 'react'
import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query'
import { apiGet, apiPost, apiDelete } from '@/api/client'
import { getSessionToken } from '@/api/session'
import { FormSection } from '@/components/form'
import { useToast } from '@/components/Toast'

interface MaskInfo {
  type: string
  exists: boolean
  path: string
  width?: number
  height?: number
  error?: string
}

interface Point {
  x: number
  y: number
}

interface MaskEditorProps {
  cameraId: number
}

type MaskType = 'motion' | 'privacy'
type DrawMode = 'rectangle' | 'polygon'

export function MaskEditor({ cameraId }: MaskEditorProps) {
  const { addToast } = useToast()
  const queryClient = useQueryClient()
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const containerRef = useRef<HTMLDivElement>(null)

  const [maskType, setMaskType] = useState<MaskType>('motion')
  const [drawMode, setDrawMode] = useState<DrawMode>('rectangle')
  const [isDrawing, setIsDrawing] = useState(false)
  const [startPoint, setStartPoint] = useState<Point | null>(null)
  const [currentPoint, setCurrentPoint] = useState<Point | null>(null)
  const [polygons, setPolygons] = useState<Point[][]>([])
  const [currentPolygon, setCurrentPolygon] = useState<Point[]>([])
  const [invert, setInvert] = useState(false)
  const [streamError, setStreamError] = useState(false)

  // Fetch current mask info
  const { data: maskInfo, isLoading } = useQuery({
    queryKey: ['mask', cameraId, maskType],
    queryFn: () => apiGet<MaskInfo>(`/${cameraId}/api/mask/${maskType}`),
  })

  // Stream dimensions from camera
  const [dimensions, setDimensions] = useState({ width: 640, height: 480 })

  // Save mask mutation
  const saveMutation = useMutation({
    mutationFn: (data: { polygons: Point[][]; width: number; height: number; invert: boolean }) =>
      apiPost(`/${cameraId}/api/mask/${maskType}`, data),
    onSuccess: () => {
      addToast(`${maskType === 'motion' ? 'Motion' : 'Privacy'} mask saved`, 'success')
      queryClient.invalidateQueries({ queryKey: ['mask', cameraId, maskType] })
    },
    onError: () => {
      addToast('Failed to save mask', 'error')
    },
  })

  // Delete mask mutation
  const deleteMutation = useMutation({
    mutationFn: () => apiDelete(`/${cameraId}/api/mask/${maskType}`),
    onSuccess: () => {
      addToast('Mask deleted', 'success')
      setPolygons([])
      queryClient.invalidateQueries({ queryKey: ['mask', cameraId, maskType] })
    },
    onError: () => {
      addToast('Failed to delete mask', 'error')
    },
  })

  // Get canvas coordinates from mouse event
  const getCanvasCoords = useCallback((e: React.MouseEvent<HTMLCanvasElement>): Point => {
    const canvas = canvasRef.current
    if (!canvas) return { x: 0, y: 0 }

    const rect = canvas.getBoundingClientRect()
    const scaleX = dimensions.width / rect.width
    const scaleY = dimensions.height / rect.height

    return {
      x: Math.round((e.clientX - rect.left) * scaleX),
      y: Math.round((e.clientY - rect.top) * scaleY),
    }
  }, [dimensions])

  // Draw all shapes on canvas
  const drawCanvas = useCallback(() => {
    const canvas = canvasRef.current
    if (!canvas) return

    const ctx = canvas.getContext('2d')
    if (!ctx) return

    // Clear canvas
    ctx.clearRect(0, 0, canvas.width, canvas.height)

    // Set style for mask areas
    ctx.fillStyle = 'rgba(255, 0, 0, 0.4)'
    ctx.strokeStyle = 'rgba(255, 0, 0, 0.8)'
    ctx.lineWidth = 2

    // Draw completed polygons
    polygons.forEach((polygon) => {
      if (polygon.length < 3) return
      ctx.beginPath()
      ctx.moveTo(polygon[0].x, polygon[0].y)
      polygon.slice(1).forEach((p) => ctx.lineTo(p.x, p.y))
      ctx.closePath()
      ctx.fill()
      ctx.stroke()
    })

    // Draw current polygon in progress
    if (currentPolygon.length > 0) {
      ctx.beginPath()
      ctx.moveTo(currentPolygon[0].x, currentPolygon[0].y)
      currentPolygon.slice(1).forEach((p) => ctx.lineTo(p.x, p.y))
      if (currentPoint) {
        ctx.lineTo(currentPoint.x, currentPoint.y)
      }
      ctx.stroke()

      // Draw points
      ctx.fillStyle = 'rgba(255, 255, 0, 0.8)'
      currentPolygon.forEach((p) => {
        ctx.beginPath()
        ctx.arc(p.x, p.y, 4, 0, Math.PI * 2)
        ctx.fill()
      })
    }

    // Draw rectangle in progress
    if (drawMode === 'rectangle' && isDrawing && startPoint && currentPoint) {
      ctx.fillStyle = 'rgba(255, 0, 0, 0.4)'
      ctx.strokeStyle = 'rgba(255, 0, 0, 0.8)'
      const x = Math.min(startPoint.x, currentPoint.x)
      const y = Math.min(startPoint.y, currentPoint.y)
      const w = Math.abs(currentPoint.x - startPoint.x)
      const h = Math.abs(currentPoint.y - startPoint.y)
      ctx.fillRect(x, y, w, h)
      ctx.strokeRect(x, y, w, h)
    }
  }, [polygons, currentPolygon, currentPoint, startPoint, isDrawing, drawMode])

  // Redraw when state changes
  useEffect(() => {
    drawCanvas()
  }, [drawCanvas])

  // Handle mouse down
  const handleMouseDown = useCallback((e: React.MouseEvent<HTMLCanvasElement>) => {
    const point = getCanvasCoords(e)

    if (drawMode === 'rectangle') {
      setIsDrawing(true)
      setStartPoint(point)
      setCurrentPoint(point)
    } else {
      // Polygon mode - add point
      setCurrentPolygon((prev) => [...prev, point])
    }
  }, [drawMode, getCanvasCoords])

  // Handle mouse move
  const handleMouseMove = useCallback((e: React.MouseEvent<HTMLCanvasElement>) => {
    const point = getCanvasCoords(e)
    setCurrentPoint(point)
  }, [getCanvasCoords])

  // Handle mouse up
  const handleMouseUp = useCallback(() => {
    if (drawMode === 'rectangle' && isDrawing && startPoint && currentPoint) {
      // Create rectangle as polygon
      const x1 = Math.min(startPoint.x, currentPoint.x)
      const y1 = Math.min(startPoint.y, currentPoint.y)
      const x2 = Math.max(startPoint.x, currentPoint.x)
      const y2 = Math.max(startPoint.y, currentPoint.y)

      if (x2 - x1 > 5 && y2 - y1 > 5) {
        const rect: Point[] = [
          { x: x1, y: y1 },
          { x: x2, y: y1 },
          { x: x2, y: y2 },
          { x: x1, y: y2 },
        ]
        setPolygons((prev) => [...prev, rect])
      }
    }

    setIsDrawing(false)
    setStartPoint(null)
  }, [drawMode, isDrawing, startPoint, currentPoint])

  // Complete polygon on double-click
  const handleDoubleClick = useCallback(() => {
    if (drawMode === 'polygon' && currentPolygon.length >= 3) {
      setPolygons((prev) => [...prev, currentPolygon])
      setCurrentPolygon([])
    }
  }, [drawMode, currentPolygon])

  // Clear current drawing
  const handleClear = useCallback(() => {
    setPolygons([])
    setCurrentPolygon([])
    setStartPoint(null)
    setCurrentPoint(null)
    setIsDrawing(false)
  }, [])

  // Undo last polygon
  const handleUndo = useCallback(() => {
    if (currentPolygon.length > 0) {
      setCurrentPolygon((prev) => prev.slice(0, -1))
    } else if (polygons.length > 0) {
      setPolygons((prev) => prev.slice(0, -1))
    }
  }, [currentPolygon.length, polygons.length])

  // Save mask
  const handleSave = useCallback(() => {
    if (polygons.length === 0) {
      addToast('Draw at least one mask area first', 'warning')
      return
    }

    saveMutation.mutate({
      polygons,
      width: dimensions.width,
      height: dimensions.height,
      invert,
    })
  }, [polygons, dimensions, invert, saveMutation, addToast])

  // Delete mask
  const handleDelete = useCallback(() => {
    if (window.confirm(`Delete the ${maskType} mask? This cannot be undone.`)) {
      deleteMutation.mutate()
    }
  }, [maskType, deleteMutation])

  // Handle stream image load to get dimensions
  const handleStreamLoad = useCallback((e: React.SyntheticEvent<HTMLImageElement>) => {
    const img = e.currentTarget
    setDimensions({ width: img.naturalWidth, height: img.naturalHeight })
    setStreamError(false)
  }, [])

  const streamUrl = useMemo(() => {
    const token = getSessionToken()
    const baseUrl = `/${cameraId}/mjpg/stream`
    return token ? `${baseUrl}?token=${encodeURIComponent(token)}` : baseUrl
  }, [cameraId])

  return (
    <FormSection
      title="Mask Editor"
      description="Draw mask areas on the camera feed to define motion detection or privacy zones"
      collapsible
      defaultOpen={false}
    >
      {/* Mask Type Selector */}
      <div className="flex gap-4 mb-4">
        <div>
          <label className="block text-sm font-medium mb-2">Mask Type</label>
          <div className="flex gap-2">
            <button
              onClick={() => {
                setMaskType('motion')
                handleClear()
              }}
              className={`px-3 py-1.5 rounded text-sm transition-colors ${
                maskType === 'motion'
                  ? 'bg-primary text-white'
                  : 'bg-surface-elevated hover:bg-surface'
              }`}
            >
              Motion
            </button>
            <button
              onClick={() => {
                setMaskType('privacy')
                handleClear()
              }}
              className={`px-3 py-1.5 rounded text-sm transition-colors ${
                maskType === 'privacy'
                  ? 'bg-primary text-white'
                  : 'bg-surface-elevated hover:bg-surface'
              }`}
            >
              Privacy
            </button>
          </div>
        </div>

        <div>
          <label className="block text-sm font-medium mb-2">Draw Mode</label>
          <div className="flex gap-2">
            <button
              onClick={() => setDrawMode('rectangle')}
              className={`px-3 py-1.5 rounded text-sm transition-colors ${
                drawMode === 'rectangle'
                  ? 'bg-primary text-white'
                  : 'bg-surface-elevated hover:bg-surface'
              }`}
            >
              Rectangle
            </button>
            <button
              onClick={() => setDrawMode('polygon')}
              className={`px-3 py-1.5 rounded text-sm transition-colors ${
                drawMode === 'polygon'
                  ? 'bg-primary text-white'
                  : 'bg-surface-elevated hover:bg-surface'
              }`}
            >
              Polygon
            </button>
          </div>
        </div>

        <div className="flex items-end">
          <label className="flex items-center gap-2 text-sm">
            <input
              type="checkbox"
              checked={invert}
              onChange={(e) => setInvert(e.target.checked)}
              className="rounded"
            />
            Invert (detect outside areas)
          </label>
        </div>
      </div>

      {/* Current Mask Status */}
      {isLoading ? (
        <div className="text-sm text-gray-400 mb-4">Loading mask info...</div>
      ) : maskInfo?.exists ? (
        <div className="text-sm text-green-500 mb-4">
          Current mask: {maskInfo.path} ({maskInfo.width}x{maskInfo.height})
        </div>
      ) : (
        <div className="text-sm text-gray-400 mb-4">No {maskType} mask configured</div>
      )}

      {/* Canvas Container */}
      <div ref={containerRef} className="relative bg-black rounded-lg overflow-hidden mb-4">
        {/* Background stream image */}
        {streamError ? (
          <div className="aspect-video bg-surface flex items-center justify-center text-gray-400">
            <div className="text-center">
              <p>Camera stream unavailable</p>
              <p className="text-xs mt-1">Draw on blank canvas (640x480)</p>
            </div>
          </div>
        ) : (
          <img
            src={streamUrl}
            alt="Camera stream"
            onLoad={handleStreamLoad}
            onError={() => setStreamError(true)}
            className="w-full h-auto"
            style={{ display: 'block' }}
          />
        )}

        {/* Drawing canvas overlay */}
        <canvas
          ref={canvasRef}
          width={dimensions.width}
          height={dimensions.height}
          onMouseDown={handleMouseDown}
          onMouseMove={handleMouseMove}
          onMouseUp={handleMouseUp}
          onMouseLeave={handleMouseUp}
          onDoubleClick={handleDoubleClick}
          className="absolute inset-0 w-full h-full cursor-crosshair"
          style={{ touchAction: 'none' }}
        />
      </div>

      {/* Instructions */}
      <div className="text-xs text-gray-400 mb-4">
        {drawMode === 'rectangle' ? (
          <p>Click and drag to draw rectangles. Red areas will be {maskType === 'motion' ? 'ignored for motion detection' : 'blacked out for privacy'}.</p>
        ) : (
          <p>Click to add polygon points. Double-click to complete the polygon.</p>
        )}
      </div>

      {/* Action Buttons */}
      <div className="flex gap-3 flex-wrap">
        <button
          onClick={handleUndo}
          disabled={polygons.length === 0 && currentPolygon.length === 0}
          className="px-3 py-1.5 text-sm bg-surface-elevated hover:bg-surface rounded transition-colors disabled:opacity-50"
        >
          Undo
        </button>
        <button
          onClick={handleClear}
          disabled={polygons.length === 0 && currentPolygon.length === 0}
          className="px-3 py-1.5 text-sm bg-surface-elevated hover:bg-surface rounded transition-colors disabled:opacity-50"
        >
          Clear All
        </button>
        <button
          onClick={handleSave}
          disabled={saveMutation.isPending || polygons.length === 0}
          className="px-4 py-1.5 text-sm bg-primary hover:bg-primary-hover rounded transition-colors disabled:opacity-50"
        >
          {saveMutation.isPending ? 'Saving...' : 'Save Mask'}
        </button>
        {maskInfo?.exists && (
          <button
            onClick={handleDelete}
            disabled={deleteMutation.isPending}
            className="px-3 py-1.5 text-sm bg-red-600 hover:bg-red-700 rounded transition-colors disabled:opacity-50"
          >
            {deleteMutation.isPending ? 'Deleting...' : 'Delete Mask'}
          </button>
        )}
      </div>

      {/* Polygon count */}
      {polygons.length > 0 && (
        <div className="text-xs text-gray-400 mt-3">
          {polygons.length} mask area{polygons.length !== 1 ? 's' : ''} drawn
        </div>
      )}
    </FormSection>
  )
}
