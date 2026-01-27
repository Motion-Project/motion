import { useState, useCallback, useRef, type RefObject } from 'react'

interface UseSheetGesturesProps {
  isOpen: boolean
  onClose: () => void
  sheetRef: RefObject<HTMLDivElement | null>
  closeThreshold?: number // percentage of sheet height to trigger close
}

interface UseSheetGesturesReturn {
  handlers: {
    onTouchStart: (e: React.TouchEvent) => void
    onTouchMove: (e: React.TouchEvent) => void
    onTouchEnd: (e: React.TouchEvent) => void
    onMouseDown: (e: React.MouseEvent) => void
  }
  style: {
    transform: string
  }
  state: {
    isDragging: boolean
    dragOffset: number
  }
}

export function useSheetGestures({
  isOpen,
  onClose,
  sheetRef,
  closeThreshold = 0.3,
}: UseSheetGesturesProps): UseSheetGesturesReturn {
  const [isDragging, setIsDragging] = useState(false)
  const [dragOffset, setDragOffset] = useState(0)

  const startY = useRef(0)
  const currentY = useRef(0)
  const velocity = useRef(0)
  const lastTime = useRef(0)
  const lastY = useRef(0)

  const handleDragStart = useCallback((clientY: number) => {
    setIsDragging(true)
    startY.current = clientY
    currentY.current = clientY
    lastY.current = clientY
    lastTime.current = Date.now()
    velocity.current = 0
  }, [])

  const handleDragMove = useCallback((clientY: number) => {
    if (!isDragging) return

    const now = Date.now()
    const deltaTime = now - lastTime.current
    const deltaY = clientY - lastY.current

    // Calculate velocity (pixels per millisecond)
    if (deltaTime > 0) {
      velocity.current = deltaY / deltaTime
    }

    lastTime.current = now
    lastY.current = clientY
    currentY.current = clientY

    // Only allow dragging down (positive offset)
    const offset = Math.max(0, clientY - startY.current)
    setDragOffset(offset)
  }, [isDragging])

  const handleDragEnd = useCallback(() => {
    if (!isDragging) return

    setIsDragging(false)

    const sheetHeight = sheetRef.current?.offsetHeight || 400
    const shouldClose =
      // Dragged past threshold
      dragOffset > sheetHeight * closeThreshold ||
      // Fast swipe down
      velocity.current > 0.5

    if (shouldClose) {
      onClose()
    }

    // Reset drag offset
    setDragOffset(0)
    velocity.current = 0
  }, [isDragging, dragOffset, onClose, closeThreshold, sheetRef])

  // Touch handlers
  const onTouchStart = useCallback(
    (e: React.TouchEvent) => {
      // Only handle if touch is on the handle area (top 60px)
      const touch = e.touches[0]
      const sheetTop = sheetRef.current?.getBoundingClientRect().top || 0
      const touchRelativeY = touch.clientY - sheetTop

      if (touchRelativeY <= 60) {
        handleDragStart(touch.clientY)
      }
    },
    [handleDragStart, sheetRef]
  )

  const onTouchMove = useCallback(
    (e: React.TouchEvent) => {
      if (isDragging) {
        e.preventDefault()
        handleDragMove(e.touches[0].clientY)
      }
    },
    [isDragging, handleDragMove]
  )

  const onTouchEnd = useCallback(() => {
    handleDragEnd()
  }, [handleDragEnd])

  // Mouse handlers (for desktop testing)
  const onMouseDown = useCallback(
    (e: React.MouseEvent) => {
      // Only handle if click is on the handle area (top 60px)
      const sheetTop = sheetRef.current?.getBoundingClientRect().top || 0
      const clickRelativeY = e.clientY - sheetTop

      if (clickRelativeY <= 60) {
        handleDragStart(e.clientY)

        const handleMouseMove = (moveEvent: MouseEvent) => {
          handleDragMove(moveEvent.clientY)
        }

        const handleMouseUp = () => {
          handleDragEnd()
          document.removeEventListener('mousemove', handleMouseMove)
          document.removeEventListener('mouseup', handleMouseUp)
        }

        document.addEventListener('mousemove', handleMouseMove)
        document.addEventListener('mouseup', handleMouseUp)
      }
    },
    [handleDragStart, handleDragMove, handleDragEnd, sheetRef]
  )

  // Calculate transform based on drag state
  const transform = isOpen
    ? isDragging
      ? `translateY(${dragOffset}px)`
      : 'translateY(0)'
    : 'translateY(100%)'

  return {
    handlers: {
      onTouchStart,
      onTouchMove,
      onTouchEnd,
      onMouseDown,
    },
    style: {
      transform,
    },
    state: {
      isDragging,
      dragOffset,
    },
  }
}
