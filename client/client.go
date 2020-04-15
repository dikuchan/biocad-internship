package main

import (
	"context"
	"fmt"
	"log"
	"os"
	"strconv"

	"github.com/go-telegram-bot-api/telegram-bot-api"
	"github.com/gopcua/opcua"
	"github.com/gopcua/opcua/ua"
	"github.com/joho/godotenv"
)

type Pair struct {
	first, second float64
}

func monitorIndex(endpoint, nodeID string, lims Pair, indexCh chan<- float64) {
	ctx := context.Background()
	interval := opcua.DefaultSubscriptionInterval

	cl := opcua.NewClient(endpoint, opcua.SecurityMode(ua.MessageSecurityModeNone))
	if err := cl.Connect(ctx); err != nil {
		log.Fatal(err)
	}

	defer cl.Close()

	log.Printf("Connected to client: %v", endpoint)

	notifyCh := make(chan *opcua.PublishNotificationData)
	sub, err := cl.Subscribe(&opcua.SubscriptionParameters{
		Interval: interval,
	}, notifyCh)
	if err != nil {
		log.Fatal(err)
	}

	defer sub.Cancel()

	log.Printf("Created subscription with ID: %v", sub.SubscriptionID)

	id, err := ua.ParseNodeID(nodeID)
	if err != nil {
		log.Fatal(err)
	}

	monRequest := opcua.NewMonitoredItemCreateRequestWithDefaults(id, ua.AttributeIDValue, 0)
	res, err := sub.Monitor(ua.TimestampsToReturnBoth, monRequest)
	if err != nil || res.Results[0].StatusCode != ua.StatusOK {
		log.Fatal(err)
	}

	go sub.Run(ctx)

	/*
	 * Read from notification channel until context is cancelled or made enough iterations.
	 */
	for {
		select {
		case <-ctx.Done():
			return
		case res := <-notifyCh:
			if res.Error != nil {
				log.Printf("Monitor error: %v", res.Error)
				continue
			}
			switch x := res.Value.(type) {
			case *ua.DataChangeNotification:
				for _, item := range x.MonitoredItems {
					data := item.Value.Value.Value().(float64)
					if data < lims.first || data > lims.second {
						indexCh <- data
						log.Printf("Limits exceeded: %v", data)
					}
				}
			default:
				log.Printf("Unknown publish result: %T", res.Value)
			}
		}
	}

	close(indexCh)
}

func handleBot(token, chName string, ls Pair, indexCh <-chan float64) {
	bot, err := tgbotapi.NewBotAPI(token)
	if err != nil {
		log.Fatal(err)
	}

	log.Printf("Authorized on Telegram as: %v", bot.Self.UserName)

	/*
	 * Send message of each exceeded value in channel.
	 */
	for idx := range indexCh {
		report := fmt.Sprintf("Range between %.0f and %.0f expected.\nValue of %.2f detected.",
			ls.first,
			ls.second,
			idx)
		msg := tgbotapi.NewMessageToChannel("@" + chName, report)

		_, err := bot.Send(msg)
		if err != nil {
			log.Printf("Cannot send message: %v", err)
		}
	}
}

func main() {
	err := godotenv.Load()
	if err != nil {
		log.Fatal(err)
	}

	endpoint := os.Getenv("OPC_ENDPOINT")
	nodeID := os.Getenv("OPC_NODE")
	token := os.Getenv("TG_TOKEN")
	chName := os.Getenv("TG_CHANNEL")

	min, err := strconv.ParseFloat(os.Getenv("LIM_MIN"), 8)
	if err != nil {
		min = -1024.0
	}
	max, err := strconv.ParseFloat(os.Getenv("LIM_MAX"), 8)
	if err != nil {
		max = 1024.0
	}
	lims := Pair{min, max}

	indexCh := make(chan float64)

	go monitorIndex(endpoint, nodeID, lims, indexCh)

	handleBot(token, chName, lims, indexCh)
}
